#pragma once

/**
 * ===== Optimization #7: Improved Exception Handling =====
 * Specific exception types for better error categorization and handling.
 */

#include <stdexcept>
#include <string>

// ===== Base Application Exception =====
class ApplicationException : public std::runtime_error {
public:
    explicit ApplicationException(const std::string& message) 
        : std::runtime_error(message) {}
    virtual ~ApplicationException() = default;
};

// ===== SSH Connection Exception =====
class SSHConnectionException : public ApplicationException {
public:
    explicit SSHConnectionException(const std::string& message) 
        : ApplicationException(message) {}
};

// ===== SSH Authentication Exception =====
class SSHAuthenticationException : public ApplicationException {
public:
    explicit SSHAuthenticationException(const std::string& message) 
        : ApplicationException(message) {}
};

// ===== SSH Session Exception =====
class SSHSessionException : public ApplicationException {
public:
    explicit SSHSessionException(const std::string& message) 
        : ApplicationException(message) {}
};

// ===== Remote Command Exception =====
class RemoteCommandException : public ApplicationException {
public:
    explicit RemoteCommandException(const std::string& message) 
        : ApplicationException(message) {}
};

// ===== Config Exception =====
class ConfigException : public ApplicationException {
public:
    explicit ConfigException(const std::string& message) 
        : ApplicationException(message) {}
};

// ===== Network Exception =====
class NetworkException : public ApplicationException {
public:
    explicit NetworkException(const std::string& message) 
        : ApplicationException(message) {}
};

// ===== Resource Exception =====
class ResourceException : public ApplicationException {
public:
    explicit ResourceException(const std::string& message) 
        : ApplicationException(message) {}
};
