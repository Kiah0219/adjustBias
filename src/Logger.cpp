#include "Logger.h"

using namespace std::filesystem;

std::string Logger::getTimestampStr() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
        << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::ensureLogDirectory() {
    path logDir = "logs";
    if (!exists(logDir)) {
        create_directory(logDir);
    }
}

void Logger::logException(const std::string& exceptionType, 
                         const std::string& exceptionMsg, 
                         const std::string& context) {
    try {
        ensureLogDirectory();
        
        std::ostringstream fileNameStream;
        fileNameStream << "logs/exception_" << getTimestampStr() << ".log";
        std::string fileName = fileNameStream.str();
        
        std::ofstream logFile(fileName, std::ios::app);
        if (logFile.is_open()) {
            logFile << "\xEF\xBB\xBF"; // UTF-8 BOM
            
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            logFile << "Time: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                    << "." << std::setfill('0') << std::setw(3) << ms.count() << std::endl;
            logFile << "Exception Type: " << exceptionType << std::endl;
            if (!context.empty()) {
                logFile << "Context: " << context << std::endl;
            }
            logFile << "Exception Message: " << exceptionMsg << std::endl;
            logFile << "----------------------------------------" << std::endl;
            logFile.close();
        }
    } catch (...) {
        std::cout << "无法写入异常日志" << std::endl;
    }
}

void Logger::logException(const QString& exceptionType, 
                         const QString& exceptionMsg, 
                         const QString& context) {
    try {
        ensureLogDirectory();
        
        std::ostringstream fileNameStream;
        fileNameStream << "logs/exception_" << getTimestampStr() << ".log";
        std::string fileName = fileNameStream.str();
        
        std::ofstream logFile(fileName, std::ios::app);
        if (logFile.is_open()) {
            logFile << "\xEF\xBB\xBF"; // UTF-8 BOM
            
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            logFile << "Time: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                    << "." << std::setfill('0') << std::setw(3) << ms.count() << std::endl;
            logFile << "Exception Type: " << exceptionType.toStdString() << std::endl;
            if (!context.isEmpty()) {
                logFile << "Context: " << context.toStdString() << std::endl;
            }
            logFile << "Exception Message: " << exceptionMsg.toStdString() << std::endl;
            logFile << "----------------------------------------" << std::endl;
            logFile.close();
        }
    } catch (...) {
        std::cout << "无法写入异常日志" << std::endl;
    }
}
