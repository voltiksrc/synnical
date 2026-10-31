#include <archive.h>
#include <archive_entry.h>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "extract.hpp"
#include "output.hpp"

using std::string;

// This file extracts the archive somewhat safely
static int copy_data(struct archive *ar, struct archive *aw) {
    int r;
    const void *buff;
    size_t size;
    la_int64_t offset;

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF)
            return (ARCHIVE_OK);
        if (r < ARCHIVE_OK)
            return (r); // fuck this im not commenting it
        r = archive_write_data_block(aw, buff, size, offset);
        if (r < ARCHIVE_OK) {
            output::error(archive_error_string(aw));
            return (r);
        }
    }
}

static bool safe_symlink(const std::filesystem::path &relative,
                         const std::filesystem::path &target) {
    namespace fs = std::filesystem;

    if (target.empty() || target.is_absolute())
        return false;

    fs::path resolved = (relative.parent_path() / target).lexically_normal();

    if (resolved.is_absolute())
        return false;

    for (const auto &part : resolved) {
        if (part == "..")
            return false;
    }

    return true;
}

struct Hardlink {
    std::filesystem::path name;
    std::filesystem::path target;
};

static bool safe_archive_path(const std::filesystem::path &path) {
    if (path.empty() || path.is_absolute())
        return false;

    for (const auto &part : path) {
        if (part == "..")
            return false;
    }

    return true;
}

// Open a directory inside the extraction root without following links
static int open_link_parent(int rootfd, const std::filesystem::path &path) {
    int fd = dup(rootfd);

    if (fd < 0)
        return -1;

    for (const auto &part : path.parent_path()) {
        int next = openat(fd, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);

        close(fd);

        if (next < 0)
            return -1;

        fd = next;
    }

    return fd;
}

static bool create_hardlinks(const std::filesystem::path &builddir,
                             const std::vector<Hardlink> &links) {
    int rootfd = open(builddir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);

    if (rootfd < 0)
        return false;

    bool success = true;

    for (const auto &link : links) {
        int source = open_link_parent(rootfd, link.target);
        int dest = open_link_parent(rootfd, link.name);

        if (source < 0 || dest < 0) {
            success = false;
        } else {
            struct stat st{};

            // Only hardlink regular files, never symlinks or directories
            if (fstatat(source, link.target.filename().c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0 ||
                !S_ISREG(st.st_mode)) {
                success = false;
            } else {
                // No flags means we won't follow the source's final symlink
                success = linkat(source,
                                 link.target.filename().c_str(),
                                 dest,
                                 link.name.filename().c_str(),
                                 0) == 0;
            }
        }

        if (source >= 0)
            close(source);

        if (dest >= 0)
            close(dest);

        if (!success) {
            output::error("Failed hardlink: " + link.name.string());
            break;
        }
    }

    close(rootfd);
    return success;
}

bool extract_source(const string &archive_path, const string &name, const string &version,
                    const std::filesystem::path &destination) {
    namespace fs =
        std::filesystem; // This makes an alias for filesystem called fs, its for convenience.

    struct archive *reader =
        archive_read_new(); // Create the reader which opens and looks inside archives,
    struct archive *writer =
        archive_write_disk_new(); // and create the writer which writes the data and extracts.
    int flags{};

    // Make sure libarchive actually initialized before we do anything.
    if (!reader || !writer) {
        output::error("Failed to initialize libarchive.");
        if (reader)
            archive_read_free(reader);
        if (writer)
            archive_write_free(writer);
        return false;
    }

    flags |= ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
    flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;
    flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
    flags |= ARCHIVE_EXTRACT_NO_OVERWRITE;

    archive_read_support_filter_all(reader);
    archive_read_support_format_all(
        reader); // These are flags to enable compression, and different archive formats.
    archive_write_disk_set_options(writer, flags);

    // Make the build directories named by the package so no package gets overridden.
    fs::path builddir = destination / (name + "-" + version);

    // Remove a previous build directory so the package can be rebuilt.
    std::error_code ec;

    auto status = fs::symlink_status(builddir, ec);

    if (ec && ec != std::errc::no_such_file_or_directory) {
        output::error("Failed to inspect build directory: " + ec.message());
        archive_read_free(reader);
        archive_write_free(writer);
        return false;
    }

    if (fs::exists(status)) {
        output::stage("Cleaning previous build directory...");

        fs::remove_all(builddir, ec);

        if (ec) {
            output::error("Failed to clean build directory: " + ec.message());
            archive_read_free(reader);
            archive_write_free(writer);
            return false;
        }
    }

    // Check that the parent directories arent symlinks either.
    fs::path parent = builddir.parent_path();
    for (fs::path path = parent; !path.empty(); path = path.parent_path()) {
        if (fs::is_symlink(fs::symlink_status(path))) {
            output::error("Unsafe build directory.");
            archive_read_free(reader);
            archive_write_free(writer);
            return false;
        }

        if (path == path.root_path())
            break;
    }

    int result = archive_read_open_filename(
        reader,
        archive_path.c_str(),
        10240); // Try open the archive and if it fails then error and exit.
    if (result != ARCHIVE_OK) {
        output::error(archive_error_string(reader));
        archive_read_free(reader);
        archive_write_free(writer);
        return false;
    }

    // Create the directory before assuming it exists.
    fs::create_directories(builddir, ec);
    if (ec) {
        output::error("Failed to create build directory: " + ec.message());
        archive_read_free(reader);
        archive_write_free(writer);
        return false;
    }

    struct archive_entry *entry;
    std::vector<Hardlink> hardlinks;
    bool success = true;

    while (true) {
        result = archive_read_next_header(reader, &entry);

        if (result == ARCHIVE_EOF)
            break;

        // If reading fails, dont pretend we extracted everything.
        if (result != ARCHIVE_OK) {
            output::error(archive_error_string(reader));
            success = false;
            break;
        }

        fs::path relative{archive_entry_pathname(entry)};

        // Reject absolute paths and empty paths so they cant escape the build directory.
        if (relative.empty() || relative.is_absolute()) {
            output::error("Invalid archive path.");
            success = false;
            break;
        }

        // Check each part of the path for .. because that could escape our directory.
        bool unsafe = false;
        for (const auto &part : relative) {
            if (part == "..") {
                unsafe = true;
                break;
            }
        }

        if (unsafe) {
            output::error("Unsafe archive path: " + relative.string());
            success = false;
            break;
        }

        // Hardlink handling:
        if (const char *target = archive_entry_hardlink(entry)) {
            fs::path link_target = target;

            if (!safe_archive_path(link_target) || !safe_archive_path(relative) ||
                link_target == relative ||
                (archive_entry_filetype(entry) != AE_IFREG && archive_entry_filetype(entry) != 0)) {
                output::error("Unsafe hardlink: " + relative.string());
                success = false;
                break;
            }

            // Save it for later in case its target hasn't been extracted yet
            hardlinks.push_back({relative, link_target});

            if (archive_read_data_skip(reader) != ARCHIVE_OK) {
                output::error("Failed to skip hardlink data.");
                success = false;
                break;
            }

            continue;
        }

        // Only extract regular files and directories for now.
        auto type = archive_entry_filetype(entry);

        if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK) {
            output::error("Unsupported archive entry: " + relative.string());
            success = false;
            break;
        }

        if (type == AE_IFLNK) {
            const char *target = archive_entry_symlink(entry);

            if (!target || !safe_symlink(relative, target)) {
                output::error("Unsafe symlink: " + relative.string());
                success = false;
                break;
            }
        }

        fs::path output = builddir / relative;
        archive_entry_set_pathname(entry, output.c_str());

        result = archive_write_header(writer, entry);
        if (result != ARCHIVE_OK) {
            output::error(archive_error_string(writer));
            success = false;
            break;
        }

        // Do copy data and if it fails then make SURE to print the error and
        // cleanup the resources such as reader/writer.
        result = copy_data(reader, writer);
        if (result != ARCHIVE_OK) {
            output::error("Failed to copy archive data.");
            success = false;
            break;
        }

        result = archive_write_finish_entry(writer); // This finalizes the extraction and metadata.
        if (result != ARCHIVE_OK) {
            output::error(archive_error_string(writer));
            success = false;
            break;
        }
    }

    // Finish writing files before linking to them
    if (archive_write_free(writer) != ARCHIVE_OK)
        success = false;

    writer = nullptr;

    if (success && !create_hardlinks(builddir, hardlinks)) {
        output::error("Failed to create archive hardlinks.");
        success = false;
    }

    // Remember to free all the resources at the end so we dont leak!!
    archive_read_free(reader);
    if (writer) {
        archive_write_free(writer);
    }
    // Dont leave an incomplete build directory if something failed.
    if (!success) {
        fs::remove_all(builddir, ec);
        if (ec)
            output::error("Failed to clean build directory: " + ec.message());
        return false;
    }

    output::info("Extraction completed successfully.");
    return true;
}
