
#pragma once
#include <iostream>    // 输入输出流
#include <sstream>     // 字符串流（stringstream）
#include <fstream>     // 文件流
#include <vector>      // 向量容器
#include <set>         // 新增：集合容器（用于parsedParams）
#include <libssh2.h>   // SSH库
#include <ws2tcpip.h>  // Windows Socket API
#include <winsock2.h>  // Windows Socket
#include <atomic>      // 原子操作
#include <Windows.h>   // Windows API
#include <memory>      // 智能指针
#include <stdexcept>   // 标准异常
#include <thread>      // 线程
#include <chrono>      // 时间库
#include <limits>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

using namespace std;




#include "SSHManager.hpp"
#include "RemoteCommandExecutor.hpp"




class ConfigReader {
private:
    SSHManager* sshManager;

    bool validateConfigFile(const string& filePath) {
        ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        // 简单验证文件内容是否为空
        string line;
        getline(file, line);
        return !line.empty();
    }

public:
    
    // 配置参数
    double xsense_data_roll = 0.0;
    double xsense_data_pitch = 0.0;
    double x_vel_offset = 0.0;
    double y_vel_offset = 0.0;
    double yaw_vel_offset = 0.0;
    double x_vel_offset_run = 0.0;
    double y_vel_offset_run = 0.0;
    double yaw_vel_offset_run = 0.0;
    double x_vel_limit_walk = numeric_limits<double>::quiet_NaN();
    double x_vel_limit_run = numeric_limits<double>::quiet_NaN();

    bool configLoaded = false;
    string configPath;
    
    // 预期参数列表
    vector<string> expectedParams = {
        "xsense_data_roll", "xsense_data_pitch", 
        "x_vel_offset", "y_vel_offset", "yaw_vel_offset",
        "x_vel_offset_run", "y_vel_offset_run", "yaw_vel_offset_run"
        // "x_vel_limit_walk", "x_vel_limit_run"
    };
    
    // 已解析的参数集合
    set<string> parsedParams;
    
    // 执行远程命令并返回输出
    string executeRemoteCommand(const string& command) {
        RemoteCommandExecutor executor(sshManager, command, false);
        executor.execute();
        
        libssh2_session_set_blocking(sshManager->getSession(), 0);
        char buffer[256];
        string result;
        int bytesRead;
        auto startTime = chrono::steady_clock::now();
        
        while (chrono::steady_clock::now() - startTime < chrono::seconds(5)) {
            bytesRead = libssh2_channel_read(executor.getChannel(), buffer, sizeof(buffer));
            if (bytesRead > 0) {
                result.append(buffer, bytesRead);
            } else if (bytesRead == 0) {
                if (libssh2_channel_eof(executor.getChannel())) {
                    break;
                }
            } else if (bytesRead == LIBSSH2_ERROR_EAGAIN) {
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
        
        return result;
    }
    
    // 创建默认配置文件
    bool createDefaultConfig() {
        try {
            qDebug() << "正在创建默认配置文件: " << configPath;
            
            // 创建目录（如果不存在）
            size_t lastSlash = configPath.find_last_of('/');
            if (lastSlash != string::npos) {
                string dirPath = configPath.substr(0, lastSlash);
                string mkdirCommand = "mkdir -p " + dirPath;
                string result = executeRemoteCommand(mkdirCommand);
                if (result.find("error") != string::npos) {
                    cerr << "创建目录失败: " << dirPath << endl;
                }
            }
            
            // 默认配置文件内容
            string defaultConfig = 
                "xsense_data_roll=0.0\n"
                "xsense_data_pitch=0.0\n"
                "x_vel_offset=0.0\n"
                "y_vel_offset=0.0\n"
                "yaw_vel_offset=0.0\n"
                "x_vel_offset_run=0.0\n"
                "y_vel_offset_run=0.0\n"
                "yaw_vel_offset_run=0.0\n"
                ;
            
            // 创建配置文件
            string createCommand = "cat > " + configPath + " << 'EOF'\n" + defaultConfig + "EOF";
            string result = executeRemoteCommand(createCommand);
            
            // 检查文件是否创建成功
            string checkCommand = "test -f " + configPath + " && echo \"created\" || echo \"failed\"";
            result = executeRemoteCommand(checkCommand);
            
            if (result.find("created") != string::npos) {
                qDebug() << "默认配置文件创建成功: " << configPath;
                return true;
            } else {
                cerr << "配置文件创建失败: " << configPath << endl;
                return false;
            }
            
        } catch (const exception& e) {
            cerr << "创建配置文件时发生错误: " << e.what() << endl;
            return false;
        }
    }
    
    // 解析配置文件内容
    void parseConfigContent(const string& content) {
        parsedParams.clear(); // 清空已解析参数集合
        istringstream contentStream(content);
        string line;
        
        while (getline(contentStream, line)) {
            // 去除行首尾空白字符
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            // 跳过空行和注释行
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // 查找等号分隔符
            size_t equalPos = line.find('=');
            if (equalPos != string::npos) {
                string varName = line.substr(0, equalPos);
                string valueStr = line.substr(equalPos + 1);
                
                // 去除变量名和值的空白字符
                varName.erase(0, varName.find_first_not_of(" \t"));
                varName.erase(varName.find_last_not_of(" \t") + 1);
                valueStr.erase(0, valueStr.find_first_not_of(" \t"));
                valueStr.erase(valueStr.find_last_not_of(" \t") + 1);
                
                try {
                    double value = stod(valueStr);
                    setParameterValue(varName, value);
                    parsedParams.insert(varName); // 记录已解析的参数
                } catch (const exception& e) {
                    cerr << "解析数值失败: " << line << " - " << e.what() << endl;
                }
            }
        }
    }
    
    // 根据变量名设置参数值
    void setParameterValue(const string& varName, double value) {
        if (varName == "xsense_data_roll") {
            xsense_data_roll = value;
        } else if (varName == "xsense_data_pitch") {
            xsense_data_pitch = value;
        } else if (varName == "x_vel_offset") {
            x_vel_offset = value;
        } else if (varName == "y_vel_offset") {
            y_vel_offset = value;
        } else if (varName == "yaw_vel_offset") {
            yaw_vel_offset = value;
        } else if (varName == "x_vel_offset_run") {
            x_vel_offset_run = value;
        } else if (varName == "y_vel_offset_run") {
            y_vel_offset_run = value;
        } else if (varName == "yaw_vel_offset_run") {
            yaw_vel_offset_run = value;
        } else if (varName == "x_vel_limit_walk") {
            x_vel_limit_walk = value;
        } else if (varName == "x_vel_limit_run") {
            x_vel_limit_run = value;
        }
    }
    
    // 检查并补充缺失的参数
    bool completeMissingParameters() {
        try {
            vector<string> missingParams;
            // 跳过 x_vel_limit_walk 和 x_vel_limit_run
            for (const auto& param : expectedParams) {
                if (param == "x_vel_limit_walk" || param == "x_vel_limit_run") {                    
                    continue;
                }
                if (parsedParams.find(param) == parsedParams.end()) {
                    missingParams.push_back(param);
                }
            }

            if (missingParams.empty()) {
                qDebug() << "配置文件已包含所有预期参数，无需补充。";
                return true;
            }
            
            qDebug() << "发现缺失参数，正在补充: ";
            for (const auto& param : missingParams) {
                qDebug() << param << " ";
            }
            qDebug();
            
            // 读取当前配置文件内容
            string readCommand = "cat " + configPath;
            string fileContent = executeRemoteCommand(readCommand);
            
            // 在文件末尾添加缺失参数
            stringstream newContent;
            newContent << fileContent;
            
            // 如果原文件末尾没有换行，先添加一个换行
            if (!fileContent.empty() && fileContent.back() != '\n') {
                newContent << "\n";
            }
            
            // newContent << "\n# 自动补充的缺失参数\n";
            for (const auto& param : missingParams) {
                newContent << param << "=0.0\n";
                setParameterValue(param, 0.0); // 设置默认值
                parsedParams.insert(param);    // 标记为已解析
            }
            
            // 写回文件
            string writeCommand = "cat > " + configPath + " << 'EOF'\n" + 
                                 newContent.str() + "EOF";
            
            string result = executeRemoteCommand(writeCommand);
            
            qDebug() << "已成功补充 " << missingParams.size() << " 个缺失参数到配置文件。";
            return true;
            
        } catch (const exception& e) {
            cerr << "补充缺失参数失败: " << e.what() << endl;
            return false;
        }
    }
    
    // 写入参数值到配置文件（更新或添加）
    bool writeParameterToFile(const string& paramName, double value) {
        try {
            // 读取当前配置文件内容
            string readCommand = "cat " + configPath;
            string fileContent = executeRemoteCommand(readCommand);
            
            if (fileContent.empty()) {
                cerr << "配置文件内容为空或读取失败" << endl;
                return false;
            }
            
            // 将内容按行分割
            vector<string> lines;
            istringstream contentStream(fileContent);
            string line;
            bool paramFound = false;
            
            while (getline(contentStream, line)) {
                // 查找目标参数行
                string trimmedLine = line;
                trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));
                
                if (trimmedLine.find(paramName + "=") == 0) {
                    // 找到参数行，更新值
                    stringstream newLine;
                    newLine << paramName << "=" << value;
                    lines.push_back(newLine.str());
                    paramFound = true;
                } else {
                    lines.push_back(line);
                }
            }
            
            // 如果参数不存在，添加到文件末尾
            if (!paramFound) {
                stringstream newLine;
                newLine << paramName << "=" << value;
                lines.push_back(newLine.str());
                parsedParams.insert(paramName); // 添加到已解析集合
            }


            


            
            // 重新写入文件
            string writeCommand = "cat > " + configPath + " << 'EOF'\n";
            for (const auto& currentLine : lines) {
                writeCommand += currentLine + "\n";
            }
            writeCommand += "EOF";
            
            string result = executeRemoteCommand(writeCommand);
            
            // 更新内存中的参数值
            setParameterValue(paramName, value);
            
            qDebug() << "参数 " << paramName << " 已成功写入配置文件: " << value;
            return true;
            
        } catch (const exception& e) {
            cerr << "写入参数到文件失败: " << paramName << " - " << e.what() << endl;
            return false;
        }
    }

public:
    ConfigReader(SSHManager* manager, const string& configPath) 
        : sshManager(manager), configPath(configPath) {}
    
    // 加载配置文件 - 如果不存在则创建，存在则检查补充缺失参数
    bool loadConfig() {
        try {
            qDebug() << "正在检查远程配置文件: " << configPath;
            
            // 检查文件是否存在
            string checkCommand = "test -f " + configPath + " && echo \"existed\" || echo \"not_exist\"";
            string result = executeRemoteCommand(checkCommand);
            
            if (result.find("existed") != string::npos) {
                qDebug() << "配置文件存在，开始读取和验证...";
                
                // 读取文件内容
                string readCommand = "cat " + configPath;
                string fileContent = executeRemoteCommand(readCommand);
                
                qDebug() << "配置文件原始内容:";
                qDebug().noquote() << fileContent;
                
                // 解析文件内容
                parseConfigContent(fileContent);
                
                // 检查并补充缺失参数
                if (completeMissingParameters()) {
                    qDebug() << "配置文件验证和补充完成!";
                } else {
                    cerr << "配置文件补充失败，但将继续使用现有参数" << endl;
                }
                
                configLoaded = true;
                return true;
                
            } else {
                qDebug() << "配置文件不存在，尝试创建默认配置文件...";
                
                // 创建默认配置文件
                if (createDefaultConfig()) {
                    // 重新读取新创建的配置文件
                    string readCommand = "cat " + configPath;
                    string fileContent = executeRemoteCommand(readCommand);
                    
                    qDebug() << "新创建的配置文件内容:";
                    qDebug().noquote() << fileContent;
                    
                    // 解析文件内容
                    parseConfigContent(fileContent);
                    qDebug() << "默认配置文件创建并解析完成!";
                    configLoaded = true;
                    return true;
                } else {
                    cerr << "无法创建配置文件，使用默认参数值0.0" << endl;
                    configLoaded = false;
                    return false;
                }
            }
            
        } catch (const exception& e) {
            cerr << "加载配置文件失败: " << e.what() << endl;
            configLoaded = false;
            return false;
        }
    }


    void writeAllValuesToFile(){
        setXsenseDataRoll(this->xsense_data_roll);
        setXsenseDataPitch(this->xsense_data_pitch);
        setXVelOffset(this->x_vel_offset);
        setYVelOffset(this->y_vel_offset);
        setYawVelOffset(this->yaw_vel_offset);
        setXVelOffsetRun(this->x_vel_offset_run);
        setYVelOffsetRun(this->y_vel_offset_run);
        setYawVelOffsetRun(this->yaw_vel_offset_run);
        setXVelLimitWalk(this->x_vel_limit_walk);
        setXVelLimitRun(this->x_vel_limit_run);

    }
    
    // 获取参数值的方法
    double getXsenseDataRoll() const { return xsense_data_roll; }
    double getXsenseDataPitch() const { return xsense_data_pitch; }
    double getXVelOffset() const { return x_vel_offset; }
    double getYVelOffset() const { return y_vel_offset; }
    double getYawVelOffset() const { return yaw_vel_offset; }
    double getXVelOffsetRun() const { return x_vel_offset_run; }
    double getYVelOffsetRun() const { return y_vel_offset_run; }
    double getYawVelOffsetRun() const { return yaw_vel_offset_run; }
    double getXVelLimitWalk() const { return x_vel_limit_walk; }
    double getXVelLimitRun() const { return x_vel_limit_run; }
    
    // 设置参数值并写入文件
    bool setXsenseDataRoll(double value) { 
        return writeParameterToFile("xsense_data_roll", value); 
    }
    bool setXsenseDataPitch(double value) { 
        return writeParameterToFile("xsense_data_pitch", value); 
    }
    bool setXVelOffset(double value) { 
        return writeParameterToFile("x_vel_offset", value); 
    }
    bool setYVelOffset(double value) { 
        return writeParameterToFile("y_vel_offset", value); 
    }
    bool setYawVelOffset(double value) { 
        return writeParameterToFile("yaw_vel_offset", value); 
    }
    bool setXVelOffsetRun(double value) { 
        return writeParameterToFile("x_vel_offset_run", value); 
    }
    bool setYVelOffsetRun(double value) { 
        return writeParameterToFile("y_vel_offset_run", value); 
    }
    bool setYawVelOffsetRun(double value) {
        return writeParameterToFile("yaw_vel_offset_run", value); 
    }
    bool setXVelLimitWalk(double value) { 
        if(std::isnan(value)) {
            cerr << "警告: 试图设置 x_vel_limit_walk 为 NaN，操作被忽略。" << endl;
            return false;
        }
        return writeParameterToFile("x_vel_limit_walk", value); 
    }
    bool setXVelLimitRun(double value) { 
        if(std::isnan(value)) {
            cerr << "警告: 试图设置 x_vel_limit_run 为 NaN，操作被忽略。" << endl;
            return false;
        }
        return writeParameterToFile("x_vel_limit_run", value); 
    }
    
    // 通用的参数设置方法
    bool setParameter(const string& paramName, double value) {
        return writeParameterToFile(paramName, value);
    }
    
    // 获取配置文件路径
    string getConfigPath() const { return configPath; }
    
    // 设置新的配置文件路径
    void setConfigPath(const string& newPath) { 
        configPath = newPath; 
        configLoaded = false; // 路径改变后需要重新加载配置
        parsedParams.clear(); // 清空已解析参数记录
    }
    
    // 更新配置文件中的参数值（兼容旧接口）
    bool updateConfigParameter(const string& paramName, double value) {
        return writeParameterToFile(paramName, value);
    }
    
    // 打印所有参数值
    void printAllParameters() const {
        qDebug() << "\n=== 解析的参数值 (配置文件: " << configPath << ") ===";
        qDebug() << "xsense_data_roll: " << xsense_data_roll;
        qDebug() << "xsense_data_pitch: " << xsense_data_pitch;
        qDebug() << "x_vel_offset: " << x_vel_offset;
        qDebug() << "y_vel_offset: " << y_vel_offset;
        qDebug() << "yaw_vel_offset: " << yaw_vel_offset;
        qDebug() << "x_vel_offset_run: " << x_vel_offset_run;
        qDebug() << "y_vel_offset_run: " << y_vel_offset_run;
        qDebug() << "yaw_vel_offset_run: " << yaw_vel_offset_run;
        qDebug() << "x_vel_limit_walk: " << x_vel_limit_walk;
        qDebug() << "x_vel_limit_run: " << x_vel_limit_run;
        // 打印已解析参数统计
        qDebug() << "已解析参数数量: " << parsedParams.size() << "/" << expectedParams.size();
    }
    
    // 检查参数是否存在
    bool isParameterExists(const string& paramName) const {
        return parsedParams.find(paramName) != parsedParams.end();
    }
    
    // 获取缺失的参数列表
    vector<string> getMissingParameters() const {
        vector<string> missing;
        for (const auto& param : expectedParams) {
            if (parsedParams.find(param) == parsedParams.end()) {
                missing.push_back(param);
            }
        }
        return missing;
    }
    
    bool isConfigLoaded() const { return configLoaded; }
};
