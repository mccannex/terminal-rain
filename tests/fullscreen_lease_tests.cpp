#include "core/fullscreen_lease.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;
using Result = FullscreenLease::Result;

static int child(const std::string& executable, const std::string& mode,
                 const std::string& scope, const std::string& directory)
{
#ifdef _WIN32
    std::string command = "\"" + executable + "\" " + mode + " " + scope + " \"" + directory + "\"";
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) return 99;
    const DWORD wait = WaitForSingleObject(process.hProcess, 10000);
    DWORD code = 99;
    if (wait == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
    else { TerminateProcess(process.hProcess, 99); WaitForSingleObject(process.hProcess, 10000); }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    const pid_t pid = fork();
    if (pid == 0)
    {
        execl(executable.c_str(), executable.c_str(), mode.c_str(), scope.c_str(), directory.c_str(), nullptr);
        _exit(99);
    }
    if (pid < 0) return 99;
    int status;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return 99;
    return WEXITSTATUS(status);
#endif
}

int main(int argc, char** argv)
{
    if (argc == 4)
    {
        FullscreenLease lease;
        const auto result = lease.acquire(argv[2], argv[3]);
        if (result == Result::Busy) return 2;
        if (result == Result::Error) return 1;
        // _Exit bypasses the RAII destructor, like abrupt host termination.
        if (std::string(argv[1]) == "--crash") std::_Exit(0);
        if (std::string(argv[1]) == "--hold")
        {
            std::cout << "ready" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(20));
        }
        return 0;
    }
#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = getpid();
#endif
    const std::string scope = "test-" + std::to_string(pid);
    const fs::path directory = fs::temp_directory_path() / ("terminal-rain-tests-" + std::to_string(pid));
    fs::create_directory(directory);
    fs::permissions(directory, fs::perms::owner_all);
    const auto executable = fs::absolute(argv[0]).string();
    int failures = 0;
    auto check = [&](bool condition, const char* message) {
        if (!condition) { std::cerr << message << '\n'; ++failures; }
    };
    {
        FullscreenLease lease;
        check(lease.acquire(scope, directory.string()) == Result::Acquired, "initial ownership");
        check(child(executable, "--try", scope, directory.string()) == 2, "other process must be rejected");
        check(child(executable, "--try", scope + "-other-display", directory.string()) == 0, "different displays must coexist");
        lease.release();
        lease.release();
        check(child(executable, "--try", scope, directory.string()) == 0, "ownership must release after stop");
        check(lease.acquire(scope, directory.string()) == Result::Acquired, "view restart must reacquire");
    }
    check(child(executable, "--try", scope, directory.string()) == 0, "destruction must release ownership");
    check(child(executable, "--crash", scope, directory.string()) == 0, "crash helper must acquire ownership");
    check(child(executable, "--try", scope, directory.string()) == 0, "crashed process must not leave a stale lock");
#ifndef _WIN32
    // macOS may host sibling views in one process; independent opens must
    // still exclude a second view targeting the same monitor.
    {
        FullscreenLease first, second;
        check(first.acquire(scope, directory.string()) == Result::Acquired, "first same-process view");
        check(second.acquire(scope, directory.string()) == Result::Busy, "duplicate same-process view");
    }
    {
        FullscreenLease lease;
        check(lease.acquire(scope, (directory / "missing").string()) == Result::Error, "lock errors must fail closed");
    }
    const auto link = directory / ("terminal-rain-" + std::to_string(getuid()) + "-symlink.lock");
    fs::create_symlink(directory / "target", link);
    {
        FullscreenLease lease;
        check(lease.acquire("symlink", directory.string()) == Result::Error, "lock must not follow symlinks");
    }
#endif
    fs::remove_all(directory);
    return failures ? 1 : 0;
}
