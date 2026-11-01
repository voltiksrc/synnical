#include "process.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

#include "build_output.hpp"

int run_command(const std::vector<std::string>& args,
                const std::filesystem::path& working_dir,
                const std::vector<std::string>& environment,
                ProcessOutput mode) {
    if (args.empty()) return -1;

    // Full mode inherits the terminal. Quiet mode writes directly to the log;
    // filtered mode uses a pipe so we can display selected lines as they arrive.
    FILE* log = nullptr;
    int pipefd[2] = {-1, -1};
    if (mode != ProcessOutput::Full) {
        log = std::tmpfile();
        if (!log) { perror("tmpfile"); return -1; }
        if (mode == ProcessOutput::Filtered && pipe(pipefd) != 0) {
            perror("pipe");
            std::fclose(log);
            return -1;
        }
    }

    std::cout.flush();
    std::cerr.flush();
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        if (pipefd[0] != -1) { close(pipefd[0]); close(pipefd[1]); }
        if (log) std::fclose(log);
        return -1;
    }

    if (pid == 0) {
        if (mode == ProcessOutput::Filtered) {
            close(pipefd[0]);
            if (dup2(pipefd[1], STDOUT_FILENO) < 0 ||
                dup2(pipefd[1], STDERR_FILENO) < 0) _exit(127);
            close(pipefd[1]);
            std::fclose(log);
        } else if (log) {
            if (dup2(fileno(log), STDOUT_FILENO) < 0 ||
                dup2(fileno(log), STDERR_FILENO) < 0) _exit(127);
            std::fclose(log);
        }

        if (chdir(working_dir.c_str()) != 0) { perror("chdir"); _exit(127); }
        for (const auto& variable : environment) {
            const auto pos = variable.find('=');
            if (pos == std::string::npos || pos == 0) {
                std::fprintf(stderr, "Invalid environment variable: %s\n", variable.c_str());
                _exit(127);
            }
            if (setenv(variable.substr(0, pos).c_str(),
                       variable.substr(pos + 1).c_str(), 1) != 0) {
                perror("setenv");
                _exit(127);
            }
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        perror("execvp");
        _exit(127);
    }

    bool read_failed = false;
    if (mode == ProcessOutput::Filtered) {
        close(pipefd[1]);
        BuildOutput display;
        char buffer[4096];
        ssize_t count;

        // Drain the pipe while the child runs. Waiting first could deadlock
        // if its compiler fills the pipe's buffer.
        while ((count = read(pipefd[0], buffer, sizeof(buffer))) != 0) {
            if (count < 0) {
                if (errno == EINTR) continue;
                perror("read");
                read_failed = true;
                break;
            }
            if (std::fwrite(buffer, 1, static_cast<std::size_t>(count), log) !=
                static_cast<std::size_t>(count)) read_failed = true;
            display.feed(std::string_view(buffer, static_cast<std::size_t>(count)));
        }
        close(pipefd[0]);
        display.finish();
    }

    int status = 0;
    pid_t waited;
    do { waited = waitpid(pid, &status, 0); }
    while (waited == -1 && errno == EINTR);

    int code = -1;
    if (waited < 0) perror("waitpid");
    else if (WIFEXITED(status)) code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) code = 128 + WTERMSIG(status);

    if (log) {
        // On failure, show the full log, including anything the filter skipped.
        if (code != 0 || read_failed) {
            std::fflush(log);
            std::rewind(log);
            char buffer[4096];
            std::size_t count;
            while ((count = std::fread(buffer, 1, sizeof(buffer), log)) > 0)
                std::fwrite(buffer, 1, count, stderr);
            std::fflush(stderr);
        }
        std::fclose(log);
    }
    return read_failed ? -1 : code;
}
