#pragma once

#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// Lifetime ownership, not a PID file: kernel cleanup also covers crashes.
// Never unlink a lock file: doing so would allow two separately locked inodes.
class FullscreenLease
{
public:
    enum class Result { Acquired, Busy, Error };
    FullscreenLease() = default;
    FullscreenLease(const FullscreenLease&) = delete;
    FullscreenLease& operator=(const FullscreenLease&) = delete;
    ~FullscreenLease() { release(); }

    Result acquire(const std::string& scope, const std::string& directory = {})
    {
        if (held()) return Result::Acquired;
#ifdef _WIN32
        (void)directory;
        const std::string name = "Local\\TerminalRain.Fullscreen." + scope;
        handle_ = CreateMutexA(nullptr, FALSE, name.c_str());
        if (!handle_) return Result::Error;
        const DWORD result = WaitForSingleObject(handle_, 0);
        if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) return Result::Acquired;
        CloseHandle(handle_);
        handle_ = nullptr;
        return result == WAIT_TIMEOUT ? Result::Busy : Result::Error;
#else
        std::string base = directory;
        if (base.empty())
        {
            const char* runtime = std::getenv("XDG_RUNTIME_DIR");
            base = runtime && *runtime ? runtime : "/tmp";
        }
        const std::string path = base + "/terminal-rain-" +
            std::to_string(getuid()) + "-" + scope + ".lock";
        int fd = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0) return Result::Error;
        struct stat info;
        if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
            info.st_uid != getuid() || info.st_nlink != 1 || (info.st_mode & 0077))
        {
            close(fd);
            return Result::Error;
        }
        int result;
        do { result = flock(fd, LOCK_EX | LOCK_NB); } while (result < 0 && errno == EINTR);
        if (result == 0)
        {
            fd_ = fd;
            return Result::Acquired;
        }
        const int error = errno;
        close(fd);
        return error == EWOULDBLOCK || error == EAGAIN ? Result::Busy : Result::Error;
#endif
    }

    bool held() const
    {
#ifdef _WIN32
        return handle_ != nullptr;
#else
        return fd_ >= 0;
#endif
    }

    void release()
    {
#ifdef _WIN32
        if (handle_) { ReleaseMutex(handle_); CloseHandle(handle_); handle_ = nullptr; }
#else
        if (fd_ >= 0) { close(fd_); fd_ = -1; }
#endif
    }

private:
#ifdef _WIN32
    HANDLE handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};
