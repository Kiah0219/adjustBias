#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
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
#include "Exceptions.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

// ===== Backward Compatibility: SSHException alias =====
using SSHException = ApplicationException;

#include "SSHManager.h"
#include "RemoteCommandExecutor.h"

class ConfigReader {
private:
    SSHManager* sshManager;

    // ===== Optimization #3: Map-based parameter storage =====
    // O(1) parameter lookup using std::map instead of O(n) if-else chains
    std::map<std::string, double*> parameterMap;
    void initializeParameterMap();
    
    bool validateConfigFile(const std::string& filePath);
    void setParameterValue(const std::string& varName, double value);
    // 原子写远端配置文件（写入临时文件并mv替换）
    bool atomicWriteRemoteFile(const std::string& content);

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
    double x_vel_limit_walk = std::numeric_limits<double>::quiet_NaN();
    double x_vel_limit_run = std::numeric_limits<double>::quiet_NaN();

    bool configLoaded = false;
    std::string configPath;
    
    // 预期参数列表 - 与parameterMap保持一致
    std::vector<std::string> expectedParams = {
        "xsense_data_roll", "xsense_data_pitch", 
        "x_vel_offset", "y_vel_offset", "yaw_vel_offset",
        "x_vel_offset_run", "y_vel_offset_run", "yaw_vel_offset_run",
        "x_vel_limit_walk", "x_vel_limit_run"
    };
    
    // 已解析的参数集合
    std::set<std::string> parsedParams;
    
    ConfigReader(SSHManager* manager, const std::string& configPath);
    
    // 执行远程命令并返回输出（带重试机制）
    std::string executeRemoteCommand(const std::string& command, int maxRetries = 3);
    
    // 创建默认配置文件
    bool createDefaultConfig();
    
    // 解析配置文件内容并去重重复参数，输出去重后内容（若没有修改则返回原始内容）
    // 返回 true 表示解析成功，dedupedContent 包含去重后的最终内容
    bool parseConfigContent(const std::string& content, std::string &dedupedContent);
    
    // 检查并补充缺失的参数
    bool completeMissingParameters();
    
    // 写入参数值到配置文件（更新或添加）
    bool writeParameterToFile(const std::string& paramName, double value);
    
    // 批量写入多个参数到配置文件（优化性能）
    bool writeMultipleParametersToFile(const std::vector<std::pair<std::string, double>>& params);
    
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
    bool setParameter(const std::string& paramName, double value);
    
    // 获取配置文件路径
    std::string getConfigPath() const;
    
    // 设置新的配置文件路径
    void setConfigPath(const std::string& newPath);
    
    // 更新配置文件中的参数值（兼容旧接口）
    bool updateConfigParameter(const std::string& paramName, double value);
    
    // 打印所有参数值
    void printAllParameters() const;
    
    // 检查参数是否存在
    bool isParameterExists(const std::string& paramName) const;
    
    // 获取缺失的参数列表
    std::vector<std::string> getMissingParameters() const;
    
    bool isConfigLoaded() const;
};