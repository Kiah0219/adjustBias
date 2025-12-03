#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <QString>

class Logger {
public:
    // C++ 字符串版本
    static void logException(const std::string& exceptionType, 
                           const std::string& exceptionMsg, 
                           const std::string& context = "");
    
    // Qt QString 版本
    static void logException(const QString& exceptionType, 
                           const QString& exceptionMsg, 
                           const QString& context = "");

private:
    static std::string getTimestampStr();
    static void ensureLogDirectory();
};
