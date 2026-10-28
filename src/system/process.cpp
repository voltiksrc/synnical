#include "process.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

int run_command(
    const std::vector<std::string>& args,
    const std::filesystem::path& working_dir,
    const std::vector<std::string>& environment
) {
    if (args.empty()) {
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Child process
        if (chdir(working_dir.c_str()) != 0) {
            perror("chdir");
            _exit(127);
        }

        // Apply environment variables such as DESTDIR=/path.
        for (const auto& variable : environment) {
            auto pos = variable.find('=');

            if (pos == std::string::npos || pos == 0) {
                _exit(127);
            }

            std::string name = variable.substr(0, pos);
            std::string value = variable.substr(pos + 1);

            if (setenv(name.c_str(), value.c_str(), 1) != 0) {
                perror("setenv");
                _exit(127);
            }
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);

        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }

        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        perror("execvp");
        _exit(127);
    }

    // Parent process
    int status = 0;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return -1;
}

