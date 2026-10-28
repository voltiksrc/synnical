#include <cerrno>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <map>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "deploy.hpp"
#include "output.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char *test_root = "/tmp/omi-test-root";

struct Entry {
    fs::path source;
    fs::path relative;
    fs::file_type type;
};

// Close file descriptors when we're done with them
struct FD {
    int value = -1;

    explicit FD(int fd = -1) : value(fd) {}
    ~FD() {
        if (value >= 0)
            close(value);
    }

    FD(const FD &) = delete;
    FD &operator=(const FD &) = delete;

    operator int() const { return value; }
};

bool safe_path(const fs::path &path) {
    if (path.empty() || path.is_absolute())
        return false;

    for (const auto &part : path) {
        if (part.empty() || part == "." || part == "..")
            return false;
    }

    return true;
}

// Don't let deployment touch Gentoo yet
int open_root(const fs::path &root) {
    if (fs::absolute(root).lexically_normal() != fs::path(test_root))
        return -1;

    if (mkdir(test_root, 0755) != 0 && errno != EEXIST)
        return -1;

    return open(test_root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
}

// Walk from the root fd so a parent symlink can't redirect us
int open_parent(int rootfd, const fs::path &path, bool create,
                Deployment *deployment = nullptr) {
    if (!safe_path(path))
        return -1;

    FD current(dup(rootfd));
    if (current.value < 0)
        return -1;

    fs::path walked;

    for (const auto &part : path.parent_path()) {
        const std::string name = part.string();
        walked /= part;

        if (create) {
            if (mkdirat(current, name.c_str(), 0755) == 0) {
                if (deployment)
                    deployment->directories.push_back(walked);
            } else if (errno != EEXIST) {
                return -1;
            }
        }

        int next = openat(current, name.c_str(),
                          O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0)
            return -1;

        close(current.value);
        current.value = next;
    }

    int result = current.value;
    current.value = -1;
    return result;
}

// Relative links are fine unless they escape the install root
bool safe_link(const fs::path &relative, const fs::path &target) {
    if (target.empty() || target.is_absolute())
        return false;

    return safe_path((relative.parent_path() / target).lexically_normal());
}

bool copy_regular(const fs::path &source, int parent, const std::string &name,
                  const fs::path &installed, Deployment &deployment,
                  std::pair<dev_t, ino_t> &inode) {
    FD input(open(source.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    if (input.value < 0)
        return false;

    struct stat st {};
    if (fstat(input, &st) != 0 || !S_ISREG(st.st_mode))
        return false;

    inode = {st.st_dev, st.st_ino};

    FD output(openat(parent, name.c_str(),
                     O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                     0600));
    if (output.value < 0)
        return false;

    // Record it now so a failed write can be undone
    deployment.files.push_back(installed);

    char buffer[65536];
    while (true) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        if (count == 0)
            break;
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }

        ssize_t written = 0;
        while (written < count) {
            ssize_t n = write(output, buffer + written, count - written);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                return false;
            written += n;
        }
    }

    // Preserve normal permissions, not setuid/setgid
    return fchmod(output, st.st_mode & 0777) == 0;
}

bool check_conflicts(int rootfd, const std::vector<Entry> &entries) {
    for (const auto &entry : entries) {
        FD parent(open_parent(rootfd, entry.relative, false));
        if (parent.value < 0) {
            if (errno == ENOENT)
                continue;
            output::error("Unsafe destination: " + entry.relative.string());
            return false;
        }

        struct stat st {};
        const std::string name = entry.relative.filename().string();
        if (fstatat(parent, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0) {
            // Shared directories are okay; existing files aren't
            if (entry.type != fs::file_type::directory || !S_ISDIR(st.st_mode)) {
                output::error("File conflict: " + entry.relative.string());
                return false;
            }
        } else if (errno != ENOENT) {
            output::error("Failed to check: " + entry.relative.string());
            return false;
        }
    }

    return true;
}

} // namespace

bool rollback_deployment(const fs::path &target_root,
                         const Deployment &deployment) {
    FD root(open_root(target_root));
    if (root.value < 0)
        return false;

    bool success = true;

    // Remove our files first
    for (auto it = deployment.files.rbegin(); it != deployment.files.rend(); ++it) {
        const fs::path relative = it->relative_path();
        if (!it->is_absolute() || !safe_path(relative)) {
            success = false;
            continue;
        }

        FD parent(open_parent(root, relative, false));
        if (parent.value < 0) {
            if (errno != ENOENT)
                success = false;
            continue;
        }

        const std::string name = relative.filename().string();
        struct stat st {};
        if (fstatat(parent, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
            if (errno != ENOENT)
                success = false;
            continue;
        }

        if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) {
            success = false;
            continue;
        }

        if (unlinkat(parent, name.c_str(), 0) != 0)
            success = false;
    }

    // Only delete directories this deployment created, and only if empty
    for (auto it = deployment.directories.rbegin();
         it != deployment.directories.rend(); ++it) {
        if (!safe_path(*it)) {
            success = false;
            continue;
        }

        FD parent(open_parent(root, *it, false));
        if (parent.value < 0) {
            if (errno != ENOENT)
                success = false;
            continue;
        }

        const std::string name = it->filename().string();
        if (unlinkat(parent, name.c_str(), AT_REMOVEDIR) != 0) {
            if (errno != ENOTEMPTY && errno != EEXIST && errno != ENOENT)
                success = false;
        }
    }

    return success;
}

bool install_staged_files(const fs::path &stage_dir, const fs::path &target_root,
                          Deployment &deployment) {
    deployment.files.clear();
    deployment.directories.clear();

    FD root(open_root(target_root));
    if (root.value < 0) {
        output::error("Could not open test root");
        return false;
    }

    std::vector<Entry> entries;

    try {
        if (!fs::is_directory(fs::symlink_status(stage_dir))) {
            output::error("Invalid staging directory");
            return false;
        }

        // Check staged entries before touching the destination
        for (const auto &item : fs::recursive_directory_iterator(stage_dir)) {
            fs::path relative = item.path().lexically_relative(stage_dir);
            auto type = item.symlink_status().type();

            if (!safe_path(relative)) {
                output::error("Unsafe staged path: " + relative.string());
                return false;
            }

            if (type != fs::file_type::directory &&
                type != fs::file_type::regular &&
                type != fs::file_type::symlink) {
                output::error("Unsupported staged file: " + relative.string());
                return false;
            }

            if (type == fs::file_type::symlink &&
                !safe_link(relative, fs::read_symlink(item.path()))) {
                output::error("Unsafe symlink: " + relative.string());
                return false;
            }

            entries.push_back({item.path(), relative, type});
        }

        if (!check_conflicts(root, entries))
            return false;

        using Inode = std::pair<dev_t, ino_t>;
        std::map<Inode, fs::path> hardlinks;
        bool completed = true;

        for (const auto &entry : entries) {
            FD parent(open_parent(root, entry.relative, true, &deployment));
            if (parent.value < 0) {
                output::error("Failed to open destination directory");
                completed = false;
                break;
            }

            const std::string name = entry.relative.filename().string();
            const fs::path installed = fs::path("/") / entry.relative;
            bool success = false;

            if (entry.type == fs::file_type::directory) {
                if (mkdirat(parent, name.c_str(), 0755) == 0) {
                    deployment.directories.push_back(entry.relative);
                    success = true;
                } else {
                    success = errno == EEXIST;
                }

                if (success) {
                    FD dir(openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY |
                                                          O_NOFOLLOW | O_CLOEXEC));
                    success = dir.value >= 0;
                }
            } else if (entry.type == fs::file_type::symlink) {
                fs::path target = fs::read_symlink(entry.source);
                if (safe_link(entry.relative, target)) {
                    success = symlinkat(target.c_str(), parent, name.c_str()) == 0;
                    if (success)
                        deployment.files.push_back(installed);
                }
            } else {
                struct stat st {};
                if (lstat(entry.source.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                    Inode inode{st.st_dev, st.st_ino};
                    auto previous = hardlinks.find(inode);

                    if (previous != hardlinks.end()) {
                        FD original(open_parent(root, previous->second, false));
                        if (original.value >= 0) {
                            const std::string old_name = previous->second.filename().string();
                            struct stat old_st {};
                            if (fstatat(original, old_name.c_str(), &old_st,
                                        AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(old_st.st_mode)) {
                                success = linkat(original, old_name.c_str(),
                                                 parent, name.c_str(), 0) == 0;
                                if (success)
                                    deployment.files.push_back(installed);
                            }
                        }
                    } else {
                        Inode copied_inode{};
                        success = copy_regular(entry.source, parent, name,
                                               installed, deployment, copied_inode);
                        if (success)
                            hardlinks.emplace(copied_inode, entry.relative);
                    }
                }
            }

            if (!success) {
                output::error("Failed to deploy: " + entry.relative.string());
                completed = false;
                break;
            }

            if (entry.type != fs::file_type::directory)
                output::info("Installed " + entry.relative.string());
        }

        if (completed) {
            output::info("Merged files into root successfully");
            return true;
        }
    } catch (const std::exception &error) {
        output::error("Deployment failed: " + std::string(error.what()));
    }

    // All failed writes and new directories get cleaned up here
    output::stage("Rolling back deployment");
    close(root.value);
    root.value = -1;

    if (!rollback_deployment(target_root, deployment))
        output::error("Rollback incomplete");

    return false;
}
