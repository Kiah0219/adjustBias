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

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

using namespace std;

#include "SSHManager.h"

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
        DualStreamBuf(streambuf* screenBuf, streambuf* fileBuf);
        
        int overflow(int c) override;
        int sync() override;
    };

public:
    Logger(const string& filename);
    ~Logger();
    
    // 显式关闭日志（通常在程序结束前调用）
    void close();
};