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

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

using namespace std;




#include "SSHManager.hpp"

class Logger {
private:
    ofstream logFile;
    streambuf* coutOriginalBuf = nullptr;
    streambuf* cerrOriginalBuf = nullptr;
    unique_ptr<streambuf> dualBuf;

    // 自定义流缓冲区类
    class DualStreamBuf : public streambuf {
    private:
        streambuf* screenBuf;
        streambuf* fileBuf;
        
    public:
        DualStreamBuf(streambuf* screenBuf, streambuf* fileBuf)
            : screenBuf(screenBuf), fileBuf(fileBuf) {}
        
        int overflow(int c) override {
            if (c == EOF) return !EOF;
            screenBuf->sputc(c);
            fileBuf->sputc(c);
            return c;
        }
        
        int sync() override {
            screenBuf->pubsync();
            fileBuf->pubsync();
            return 0;
        }
    };

public:
    Logger(const string& filename) {
        // 打开日志文件
        logFile.open(filename);
        if (!logFile.is_open()) {
            throw runtime_error("无法创建日志文件: " + filename);
        }
        
        // 保存原始缓冲区
        coutOriginalBuf = cout.rdbuf();
        cerrOriginalBuf = cerr.rdbuf();
        
        // 创建双重输出缓冲区
        dualBuf = make_unique<DualStreamBuf>(coutOriginalBuf, logFile.rdbuf());
        
        // 重定向标准输出
        cout.rdbuf(dualBuf.get());
        cerr.rdbuf(dualBuf.get());
    }
    
    ~Logger() {
        // 恢复原始输出设置
        cout.rdbuf(coutOriginalBuf);
        cerr.rdbuf(cerrOriginalBuf);
        
        // 关闭日志文件
        if (logFile.is_open()) {
            logFile.close();
        }
    }
    
    // 显式关闭日志（通常在程序结束前调用）
    void close() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }
};