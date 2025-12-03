#pragma once

/**
 * ===== Optimization #4: Centralized Constants Definition =====
 * This header file consolidates all hardcoded values and constants used throughout
 * the adjustBias application, improving maintainability and consistency.
 * 
 * Benefits:
 * - Single source of truth for all configuration constants
 * - Easy to modify values without searching through codebase
 * - Reduced risk of inconsistent values across files
 * - Improved code readability through meaningful constant names
 */

#include <chrono>
#include <string>
#include <limits>

// ===== SSH Connection Constants =====
namespace SSH {
    // SSH connection timeouts (in seconds)
    constexpr int HANDSHAKE_TIMEOUT = 15;
    constexpr int AUTHENTICATION_TIMEOUT = 10;
    constexpr int COMMAND_EXECUTION_TIMEOUT = 30;
    constexpr int RECONNECTION_TIMEOUT = 5;
    
    // SSH connection retry settings
    constexpr int MAX_RETRY_ATTEMPTS = 3;
    constexpr int RETRY_DELAY_MS = 1000;  // 1 second
    
    // SSH session validity check interval (milliseconds)
    constexpr int VALIDITY_CHECK_INTERVAL_MS = 100;
    
    // Socket read/write buffer size
    constexpr int BUFFER_SIZE = 1024;
    
    // Monitor thread check interval for disconnections
    constexpr int MONITOR_CHECK_INTERVAL_MS = 500;
}

// ===== Configuration File Constants =====
namespace Config {
    // Default parameter values
    constexpr double DEFAULT_XSENSE_DATA_ROLL = 0.0;
    constexpr double DEFAULT_XSENSE_DATA_PITCH = 0.0;
    constexpr double DEFAULT_X_VEL_OFFSET = 0.0;
    constexpr double DEFAULT_Y_VEL_OFFSET = 0.0;
    constexpr double DEFAULT_YAW_VEL_OFFSET = 0.0;
    constexpr double DEFAULT_X_VEL_OFFSET_RUN = 0.0;
    constexpr double DEFAULT_Y_VEL_OFFSET_RUN = 0.0;
    constexpr double DEFAULT_YAW_VEL_OFFSET_RUN = 0.0;
    
    // Default velocity limits (in m/s)
    constexpr double DEFAULT_X_VEL_LIMIT_WALK = 0.5;
    constexpr double DEFAULT_X_VEL_LIMIT_RUN = 1.0;
    
    // NaN value for uninitialized limits
    const double UNINITIALIZED_LIMIT = std::numeric_limits<double>::quiet_NaN();
    
    // Configuration file paths
    const std::string DEFAULT_CONFIG_PATH = "/tmp/robot_config.ini";
    const std::string LOGS_DIRECTORY = "logs";
    const std::string LOG_FILE_PREFIX = "adjustBias";
}

// ===== Remote Command Constants =====
namespace RemoteCommand {
    // Command execution settings
    constexpr int MAX_RETRIES = 3;
    constexpr int TIMEOUT_SECONDS = 30;
    constexpr int RETRY_DELAY_MS = 1000;
    
    // Command templates
    const std::string MKDIR_COMMAND_TEMPLATE = "mkdir -p ";
    const std::string CAT_READ_TEMPLATE = "cat ";
    const std::string CAT_WRITE_TEMPLATE = "cat > ";
    const std::string TEST_FILE_TEMPLATE = "test -f ";
    const std::string EOF_DELIMITER = "EOF";
}

// ===== UI/Display Constants =====
namespace UI {
    // Window sizes and positions (optional defaults)
    constexpr int DEFAULT_WINDOW_WIDTH = 800;
    constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    
    // Update intervals for UI refreshes (in milliseconds)
    constexpr int STATUS_UPDATE_INTERVAL_MS = 500;
    constexpr int PARAMETER_UPDATE_INTERVAL_MS = 100;
}

// ===== Error Message Constants =====
namespace ErrorMessages {
    const std::string SSH_DISCONNECTED = "SSH connection disconnected";
    const std::string INVALID_SSH_SESSION = "Invalid SSH session obtained";
    const std::string SSH_RECONNECT_FAILED = "SSH reconnection failed";
    const std::string COMMAND_TIMEOUT = "Command execution timeout";
    const std::string INVALID_PARAMETER_VALUE = "Invalid parameter value format";
    const std::string FILE_NOT_FOUND = "Configuration file not found";
    const std::string CONFIG_PARSE_ERROR = "Failed to parse configuration file";
}

// ===== Numeric Value Validation Constants =====
namespace Validation {
    // Velocity value ranges (m/s)
    constexpr double MIN_VELOCITY = -2.0;
    constexpr double MAX_VELOCITY = 2.0;
    
    // Angle value ranges (degrees)
    constexpr double MIN_ANGLE = -180.0;
    constexpr double MAX_ANGLE = 180.0;
    
    // Offset value ranges
    constexpr double MIN_OFFSET = -1.0;
    constexpr double MAX_OFFSET = 1.0;
    
    // Precision for floating-point comparisons
    constexpr double EPSILON = 1e-6;
}

// ===== Logging Constants =====
namespace Logging {
    // Log file settings
    constexpr int MAX_LOG_FILE_SIZE_KB = 1024;  // 1 MB max per log file
    constexpr int MAX_LOG_FILES = 10;           // Keep last 10 log files
    
    // Log levels (for potential future use)
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3
    };
    
    // Timestamp format string (for strftime-like functions)
    const std::string TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S";
}

#endif // CONFIG_H
