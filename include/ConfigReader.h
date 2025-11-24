#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <set>
#include <libssh2.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <atomic>
#include <Windows.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <limits>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

using namespace std;

#include "SSHManager.h"
#include "RemoteCommandExecutor.h"

class ConfigReader {
private:
    SSHManager* sshManager;

    bool validateConfigFile(const string& filePath);
    void setParameterValue(const string& varName, double value);

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
    };
    
    // 已解析的参数集合
    set<string> parsedParams;
    
    ConfigReader(SSHManager* manager, const string& configPath);
    
    // 执行远程命令并返回输出（带重试机制）
    string executeRemoteCommand(const string& command, int maxRetries = 3);
    
    // 创建默认配置文件
    bool createDefaultConfig();
    
    // 解析配置文件内容
    void parseConfigContent(const string& content);
    
    // 检查并补充缺失的参数
    bool completeMissingParameters();
    
    // 写入参数值到配置文件（更新或添加）
    bool writeParameterToFile(const string& paramName, double value);
    
    // 加载配置文件
    bool loadConfig();
    
    void writeAllValuesToFile();
    
    // 批量更新多个参数到配置文件
    bool updateMultipleParameters(
        double xsense_data_roll, 
        double xsense_data_pitch,
        double x_vel_offset,
        double y_vel_offset,
        double yaw_vel_offset,
        double x_vel_offset_run,
        double y_vel_offset_run,
        double yaw_vel_offset_run,
        double x_vel_limit_walk,
        double x_vel_limit_run
    );
    
    // 获取参数值的方法
    double getXsenseDataRoll() const;
    double getXsenseDataPitch() const;
    double getXVelOffset() const;
    double getYVelOffset() const;
    double getYawVelOffset() const;
    double getXVelOffsetRun() const;
    double getYVelOffsetRun() const;
    double getYawVelOffsetRun() const;
    double getXVelLimitWalk() const;
    double getXVelLimitRun() const;
    
    // 设置参数值并写入文件
    bool setXsenseDataRoll(double value);
    bool setXsenseDataPitch(double value);
    bool setXVelOffset(double value);
    bool setYVelOffset(double value);
    bool setYawVelOffset(double value);
    bool setXVelOffsetRun(double value);
    bool setYVelOffsetRun(double value);
    bool setYawVelOffsetRun(double value);
    bool setXVelLimitWalk(double value);
    bool setXVelLimitRun(double value);
    
    // 通用的参数设置方法
    bool setParameter(const string& paramName, double value);
    
    // 获取配置文件路径
    string getConfigPath() const;
    
    // 设置新的配置文件路径
    void setConfigPath(const string& newPath);
    
    // 更新配置文件中的参数值（兼容旧接口）
    bool updateConfigParameter(const string& paramName, double value);
    
    // 打印所有参数值
    void printAllParameters() const;
    
    // 检查参数是否存在
    bool isParameterExists(const string& paramName) const;
    
    // 获取缺失的参数列表
    vector<string> getMissingParameters() const;
    
    bool isConfigLoaded() const;
};