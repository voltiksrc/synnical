#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "process.hpp"

int run_command(const std::vector<std::string> &args, const std::filesystem::path &working_dir,
                const std::vector<std::string> &environment, bool show_output) {
    if (args.empty())
        return -1;

    FILE *log = nullptr;

    // Quiet mode: save the command's output instead of printing it.
    // If the command fails, we'll print the saved output afterward.
    if (!show_output) {
        log = std::tmpfile();

        if (!log) {
            perror("tmpfile");
            return -1;
        }
    }

    // Flush existing output before forking so it isn't duplicated.
    std::cout.flush();
    std::cerr.flush();

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");

        if (log)
            std::fclose(log);

        return -1;
    }

    if (pid == 0) {
        // Child process: this is where the actual command runs.

        // Send stdout and stderr to the temporary log in quiet mode.
        if (log) {
            int fd = fileno(log);

            if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1) {
                _exit(127);
            }

            std::fclose(log);
        }

        // Run the command from its requested directory.
        if (chdir(working_dir.c_str()) != 0) {
            perror("chdir");
            _exit(127);
        }

        // Apply environment variables such as DESTDIR=/path.
        // These changes only affect the child, not Synnical itself.
        for (const auto &variable : environment) {
            auto pos = variable.find('=');

            if (pos == std::string::npos || pos == 0) {
                std::fprintf(stderr, "Invalid environment variable: %s\n", variable.c_str());
                _exit(127);
            }

            std::string name = variable.substr(0, pos);
            std::string value = variable.substr(pos + 1);

            if (setenv(name.c_str(), value.c_str(), 1) != 0) {
                perror("setenv");
                _exit(127);
            }
        }

        // execvp() expects a null-terminated array of C strings.
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);

        for (const auto &arg : args)
            argv.push_back(const_cast<char *>(arg.c_str()));

        argv.push_back(nullptr);

        // Replace the child process with the requested program.
        // If execvp succeeds, this function never returns.
        execvp(argv[0], argv.data());

        perror("execvp");
        _exit(127);
    }

    // Parent process: wait until the command finishes.
    int status = 0;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    int exit_code = -1;

    if (result == -1) {
        perror("waitpid");
    } else if (WIFEXITED(status)) {
        // The command exited normally.
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        // The command was terminated by a signal.
        exit_code = 128 + WTERMSIG(status);
    }

    // In quiet mode, only replay the compiler/build logs on failure.
    if (log) {
        if (exit_code != 0) {
            std::rewind(log);

            char buffer[4096];
            std::size_t count = 0;

            while ((count = std::fread(buffer, 1, sizeof(buffer), log)) > 0) {
                std::fwrite(buffer, 1, count, stderr);
            }

            std::fflush(stderr);
        }

        // Closing the temporary file also removes it.
        std::fclose(log);
    }

    return exit_code;
}
