#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include "Exceptions.h"

// ===== Optimization #9: Parameter Validation Framework =====
// Purpose: Centralized validation for all input parameters
// Benefits:
//   - Consistent validation across codebase
//   - Early detection of invalid parameters
//   - Cleaner code without scattered validation logic
//   - Type-safe validation with compile-time checks

namespace Validation {

// ===== Validation Result Type =====
struct ValidationResult {
    bool isValid;
    std::string errorMessage;
    
    ValidationResult(bool valid, const std::string& msg = "") 
        : isValid(valid), errorMessage(msg) {}
    
    explicit operator bool() const {
        return isValid;
    }
};

// ===== String Validators =====
class StringValidator {
public:
    // Validate hostname/IP address format
    static ValidationResult validateHostname(const std::string& hostname) {
        if (hostname.empty()) {
            return ValidationResult(false, "Hostname cannot be empty");
        }
        if (hostname.length() > 255) {
            return ValidationResult(false, "Hostname exceeds 255 characters");
        }
        
        // Check for valid characters: alphanumeric, dots, hyphens
        for (char c : hostname) {
            if (!isalnum(c) && c != '.' && c != '-') {
                return ValidationResult(false, "Hostname contains invalid characters");
            }
        }
        
        return ValidationResult(true);
    }
    
    // Validate port number
    static ValidationResult validatePort(int port) {
        if (port < 1 || port > 65535) {
            return ValidationResult(false, "Port must be between 1 and 65535");
        }
        return ValidationResult(true);
    }
    
    // Validate SSH port (commonly 22)
    static ValidationResult validateSSHPort(int port) {
        // Allow common SSH ports
        std::vector<int> validPorts = {22, 2222, 2222, 10022};
        
        for (int validPort : validPorts) {
            if (port == validPort) {
                return ValidationResult(true);
            }
        }
        
        // Also allow any port in valid range
        if (port >= 1 && port <= 65535) {
            return ValidationResult(true);
        }
        
        return ValidationResult(false, "Invalid SSH port");
    }
    
    // Validate username
    static ValidationResult validateUsername(const std::string& username) {
        if (username.empty()) {
            return ValidationResult(false, "Username cannot be empty");
        }
        if (username.length() > 32) {
            return ValidationResult(false, "Username exceeds 32 characters");
        }
        
        // Allow alphanumeric and common special characters
        for (char c : username) {
            if (!isalnum(c) && c != '_' && c != '-' && c != '.') {
                return ValidationResult(false, "Username contains invalid characters");
            }
        }
        
        return ValidationResult(true);
    }
    
    // Validate password
    static ValidationResult validatePassword(const std::string& password) {
        if (password.empty()) {
            return ValidationResult(false, "Password cannot be empty");
        }
        if (password.length() > 256) {
            return ValidationResult(false, "Password exceeds 256 characters");
        }
        return ValidationResult(true);
    }
    
    // Validate file path
    static ValidationResult validateFilePath(const std::string& path) {
        if (path.empty()) {
            return ValidationResult(false, "File path cannot be empty");
        }
        if (path.length() > 4096) {
            return ValidationResult(false, "File path exceeds maximum length");
        }
        
        // Check for null characters
        if (path.find('\0') != std::string::npos) {
            return ValidationResult(false, "File path contains null characters");
        }
        
        return ValidationResult(true);
    }
    
    // Validate command string
    static ValidationResult validateCommand(const std::string& command) {
        if (command.empty()) {
            return ValidationResult(false, "Command cannot be empty");
        }
        if (command.length() > 2048) {
            return ValidationResult(false, "Command exceeds maximum length");
        }
        return ValidationResult(true);
    }
};

// ===== Numeric Validators =====
class NumericValidator {
public:
    // Validate timeout value
    static ValidationResult validateTimeout(int timeoutMs) {
        if (timeoutMs <= 0) {
            return ValidationResult(false, "Timeout must be positive");
        }
        if (timeoutMs > 600000) { // 10 minutes max
            return ValidationResult(false, "Timeout exceeds 10 minutes");
        }
        return ValidationResult(true);
    }
    
    // Validate retry count
    static ValidationResult validateRetryCount(int retries) {
        if (retries < 0) {
            return ValidationResult(false, "Retry count cannot be negative");
        }
        if (retries > 10) {
            return ValidationResult(false, "Retry count exceeds maximum (10)");
        }
        return ValidationResult(true);
    }
    
    // Validate buffer size
    static ValidationResult validateBufferSize(size_t size) {
        if (size == 0) {
            return ValidationResult(false, "Buffer size cannot be zero");
        }
        if (size > 10485760) { // 10 MB max
            return ValidationResult(false, "Buffer size exceeds 10 MB");
        }
        return ValidationResult(true);
    }
    
    // Validate interval value
    static ValidationResult validateInterval(int intervalMs) {
        if (intervalMs <= 0) {
            return ValidationResult(false, "Interval must be positive");
        }
        if (intervalMs > 300000) { // 5 minutes max
            return ValidationResult(false, "Interval exceeds 5 minutes");
        }
        return ValidationResult(true);
    }
};

// ===== Composite Validator for SSH Connection Parameters =====
class SSHConnectionValidator {
public:
    struct SSHParams {
        std::string hostname;
        int port;
        std::string username;
        std::string password;
    };
    
    static ValidationResult validateAll(const SSHParams& params) {
        // Validate hostname
        auto hostnameResult = StringValidator::validateHostname(params.hostname);
        if (!hostnameResult) {
            throw ConfigException("Hostname validation failed: " + hostnameResult.errorMessage);
        }
        
        // Validate port
        auto portResult = StringValidator::validatePort(params.port);
        if (!portResult) {
            throw ConfigException("Port validation failed: " + portResult.errorMessage);
        }
        
        // Validate username
        auto usernameResult = StringValidator::validateUsername(params.username);
        if (!usernameResult) {
            throw ConfigException("Username validation failed: " + usernameResult.errorMessage);
        }
        
        // Validate password
        auto passwordResult = StringValidator::validatePassword(params.password);
        if (!passwordResult) {
            throw ConfigException("Password validation failed: " + passwordResult.errorMessage);
        }
        
        return ValidationResult(true);
    }
};

// ===== Custom Validator Template =====
template<typename T>
class CustomValidator {
private:
    std::function<bool(const T&)> validationFunc;
    std::string errorMsg;

public:
    CustomValidator(std::function<bool(const T&)> func, const std::string& msg)
        : validationFunc(func), errorMsg(msg) {}
    
    ValidationResult validate(const T& value) {
        try {
            if (validationFunc(value)) {
                return ValidationResult(true);
            }
            return ValidationResult(false, errorMsg);
        } catch (const std::exception& e) {
            return ValidationResult(false, std::string("Validation error: ") + e.what());
        }
    }
};

} // namespace Validation
