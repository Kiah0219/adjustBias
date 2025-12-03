#pragma once

#include <functional>
#include <memory>
#include <libssh2.h>
#include <winsock2.h>
#include "Exceptions.h"

// ===== Optimization #8: RAII Resource Management =====
// Purpose: Ensure automatic cleanup of resources with strong exception safety
// Benefits:
//   - No resource leaks even on exception
//   - Automatic cleanup on scope exit
//   - Strong exception safety guarantees
//   - Cleaner code without manual cleanup calls

namespace ResourceManagement {

// ===== LibSSH2 Channel RAII Wrapper =====
class ChannelGuard {
private:
    LIBSSH2_CHANNEL* channel;
    LIBSSH2_SESSION* session;
    
    void cleanup() noexcept {
        if (channel && session) {
            try {
                libssh2_channel_send_eof(channel);
                libssh2_channel_close(channel);
                libssh2_channel_free(channel);
            } catch (...) {
                // Suppress exceptions during cleanup
            }
            channel = nullptr;
        }
    }

public:
    // Constructor - takes ownership of channel
    explicit ChannelGuard(LIBSSH2_CHANNEL* ch, LIBSSH2_SESSION* sess) noexcept 
        : channel(ch), session(sess) {
        if (!channel) {
            throw ResourceException("ChannelGuard: Invalid channel pointer");
        }
        if (!session) {
            throw ResourceException("ChannelGuard: Invalid session pointer");
        }
    }

    // Destructor - releases channel
    ~ChannelGuard() noexcept {
        cleanup();
    }

    // Disable copy
    ChannelGuard(const ChannelGuard&) = delete;
    ChannelGuard& operator=(const ChannelGuard&) = delete;

    // Move semantics
    ChannelGuard(ChannelGuard&& other) noexcept 
        : channel(other.channel), session(other.session) {
        other.channel = nullptr;
        other.session = nullptr;
    }

    ChannelGuard& operator=(ChannelGuard&& other) noexcept {
        if (this != &other) {
            cleanup();
            channel = other.channel;
            session = other.session;
            other.channel = nullptr;
            other.session = nullptr;
        }
        return *this;
    }

    // Get raw pointer
    LIBSSH2_CHANNEL* get() const noexcept {
        return channel;
    }

    // Arrow operator for convenience
    LIBSSH2_CHANNEL* operator->() const noexcept {
        return channel;
    }

    // Dereference operator
    LIBSSH2_CHANNEL& operator*() const noexcept {
        return *channel;
    }

    // Explicit release (relinquish ownership)
    LIBSSH2_CHANNEL* release() noexcept {
        LIBSSH2_CHANNEL* temp = channel;
        channel = nullptr;
        return temp;
    }

    // Check validity
    explicit operator bool() const noexcept {
        return channel != nullptr;
    }
};

// ===== Generic RAII Wrapper =====
// Usage: GenericGuard<FILE*> fileGuard(fopen(...), [](FILE* f) { fclose(f); });
template<typename T>
class GenericGuard {
private:
    T resource;
    std::function<void(T)> deleter;
    bool owns_resource;

public:
    // Constructor with custom deleter
    GenericGuard(T res, std::function<void(T)> del) 
        : resource(res), deleter(del), owns_resource(true) {
        if (!res) {
            throw ResourceException("GenericGuard: Invalid resource pointer");
        }
    }

    // Destructor
    ~GenericGuard() noexcept {
        if (owns_resource && resource) {
            try {
                deleter(resource);
            } catch (...) {
                // Suppress exceptions during cleanup
            }
        }
    }

    // Disable copy
    GenericGuard(const GenericGuard&) = delete;
    GenericGuard& operator=(const GenericGuard&) = delete;

    // Move semantics
    GenericGuard(GenericGuard&& other) noexcept 
        : resource(other.resource), deleter(other.deleter), owns_resource(other.owns_resource) {
        other.owns_resource = false;
    }

    GenericGuard& operator=(GenericGuard&& other) noexcept {
        if (this != &other) {
            if (owns_resource && resource) {
                try {
                    deleter(resource);
                } catch (...) {}
            }
            resource = other.resource;
            deleter = other.deleter;
            owns_resource = other.owns_resource;
            other.owns_resource = false;
        }
        return *this;
    }

    // Get raw pointer
    T get() const noexcept {
        return resource;
    }

    // Arrow operator
    T operator->() const noexcept {
        return resource;
    }

    // Release ownership
    T release() noexcept {
        owns_resource = false;
        return resource;
    }

    // Check validity
    explicit operator bool() const noexcept {
        return resource != nullptr;
    }
};

// ===== Socket RAII Wrapper =====
class SocketGuard {
private:
    SOCKET sock;

    void cleanup() noexcept {
        if (sock != INVALID_SOCKET) {
            try {
                closesocket(sock);
            } catch (...) {
                // Suppress exceptions during cleanup
            }
            sock = INVALID_SOCKET;
        }
    }

public:
    explicit SocketGuard(SOCKET s = INVALID_SOCKET) noexcept : sock(s) {
        if (s != INVALID_SOCKET && s == 0) {
            throw NetworkException("SocketGuard: Invalid socket");
        }
    }

    ~SocketGuard() noexcept {
        cleanup();
    }

    // Disable copy
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;

    // Move semantics
    SocketGuard(SocketGuard&& other) noexcept : sock(other.sock) {
        other.sock = INVALID_SOCKET;
    }

    SocketGuard& operator=(SocketGuard&& other) noexcept {
        if (this != &other) {
            cleanup();
            sock = other.sock;
            other.sock = INVALID_SOCKET;
        }
        return *this;
    }

    SOCKET get() const noexcept {
        return sock;
    }

    SOCKET release() noexcept {
        SOCKET temp = sock;
        sock = INVALID_SOCKET;
        return temp;
    }

    explicit operator bool() const noexcept {
        return sock != INVALID_SOCKET;
    }

    // Assign new socket
    void reset(SOCKET s = INVALID_SOCKET) noexcept {
        cleanup();
        sock = s;
    }
};

// ===== Scope Guard for arbitrary cleanup functions =====
// Usage: ScopeGuard guard([]() { /* cleanup code */ });
class ScopeGuard {
private:
    std::function<void()> cleanup_func;
    bool engaged;

public:
    explicit ScopeGuard(std::function<void()> func) 
        : cleanup_func(func), engaged(true) {
        if (!func) {
            throw ResourceException("ScopeGuard: Invalid cleanup function");
        }
    }

    ~ScopeGuard() noexcept {
        if (engaged) {
            try {
                cleanup_func();
            } catch (...) {
                // Suppress exceptions
            }
        }
    }

    // Disable copy
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    // Move semantics
    ScopeGuard(ScopeGuard&& other) noexcept 
        : cleanup_func(other.cleanup_func), engaged(other.engaged) {
        other.engaged = false;
    }

    ScopeGuard& operator=(ScopeGuard&& other) noexcept {
        if (this != &other) {
            if (engaged) {
                try {
                    cleanup_func();
                } catch (...) {}
            }
            cleanup_func = other.cleanup_func;
            engaged = other.engaged;
            other.engaged = false;
        }
        return *this;
    }

    // Dismiss guard (prevent cleanup)
    void dismiss() noexcept {
        engaged = false;
    }

    // Check if guard is active
    explicit operator bool() const noexcept {
        return engaged;
    }
};

} // namespace ResourceManagement
