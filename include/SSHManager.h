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
#include <mutex>
#include <QDebug>
#include <iomanip>
#include <filesystem>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

using namespace std;
using namespace std::filesystem;

// 全局中断标志
static atomic<bool> g_interrupted(false);

// Ctrl+C处理函数
inline BOOL WINAPI ConsoleHandler(DWORD dwCtrlType);

// 异常类
class SSHException : public runtime_error {
public:
    SSHException(const string& msg);
};

// SSH连接管理类
class SSHManager {
private:
    SOCKET sock = INVALID_SOCKET;
    LIBSSH2_SESSION* session = nullptr;
    string host;
    string username;
    string password;
    int port;
    bool sessionValid = false;

    // 监控线程
    std::thread monitorThread;
    std::atomic<bool> monitorRunning{false};
    int monitorIntervalMs = 30000; // 改为30秒检测一次，减少开销
    std::mutex sessionMutex; // 添加会话互斥锁
    std::weak_ptr<SSHManager> weakThis; // 用于线程安全的生命周期管理
    
    void cleanup();
    void connectSocket();
    void initializeSSH();
    
    // 异常日志记录
    static void logException(const string& exceptionType, const string& exceptionMsg, const string& context = "");
    
    // 检查会话是否仍然有效
    bool checkSessionValidity();

    // 检查底层 socket 是否已断开（peek 不会从缓冲区移除数据）
    bool checkSocketDisconnected();

    // 重新建立SSH连接
    void reconnect();

public:
    SSHManager(const string& host, const string& username, 
               const string& password, int port = 22);

    // 禁用拷贝构造函数和赋值运算符
    SSHManager(const SSHManager&) = delete;
    SSHManager& operator=(const SSHManager&) = delete;
    
    // 支持移动语义
    SSHManager(SSHManager&& other) noexcept;
    SSHManager& operator=(SSHManager&& other) noexcept;
    
    ~SSHManager();

    LIBSSH2_SESSION* getSession();
    
    string getPassword();
    
    string getHost();
    bool isSessionValid();
    
    // 对外接口：判断 SSH 是否断开（会检查 session 有效性和底层 socket）
    // 返回 true 表示已断开或不可用，false 表示连接看起来还活着
    bool isSSHDisconnected();
    
    // 强制设置会话为无效（用于模拟中断后的状态）
    void invalidateSession();
};