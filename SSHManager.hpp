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
#include <mutex>       // 互斥量

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

using namespace std;




// 全局中断标志
static atomic<bool> g_interrupted(false);

// Ctrl+C处理函数
inline BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT) {
        g_interrupted = true;
        return TRUE;
    }
    return FALSE;
}

// 异常类
class SSHException : public runtime_error {
public:
    SSHException(const string& msg) : runtime_error(msg) {}
};


// SSH连接管理类
class SSHManager {
private:
    SOCKET sock = INVALID_SOCKET;
    LIBSSH2_SESSION* session = nullptr;
    string host;

    void cleanup() {
        if (session) {
            libssh2_session_disconnect(session, "Normal shutdown");
            libssh2_session_free(session);
            session = nullptr;
        }
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }
    string username;
    string password;
    int port;
    bool sessionValid = false;

    // 监控线程
    std::thread monitorThread;
    std::atomic<bool> monitorRunning{false};
    int monitorIntervalMs = 5000; // 默认每5秒检测一次

public:
    void connectSocket() {
        // 创建Socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            throw SSHException("Socket creation failed: " + to_string(WSAGetLastError()));
        }

        // 设置服务器地址
        sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &sin.sin_addr) != 1) {
            closesocket(sock);
            throw SSHException("Invalid address: " + host);
        }

        // 连接服务器
        if (connect(sock, (struct sockaddr*)(&sin), sizeof(sin))) {
            closesocket(sock);
            throw SSHException("Connection failed: " + to_string(WSAGetLastError()));
        }
    }

    void initializeSSH() {
        // 初始化libssh2
        if (libssh2_init(0)) {
            closesocket(sock);
            throw SSHException("libssh2 initialization failed");
        }

        // 创建SSH会话
        session = libssh2_session_init();
        if (!session) {
            closesocket(sock);
            libssh2_exit();
            throw SSHException("Failed to create SSH session");
        }

        // 设置会话为阻塞模式（文件操作需要）
        libssh2_session_set_blocking(session, 1);

        // 握手
        if (libssh2_session_handshake(session, sock)) {
            string error = "SSH handshake failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            libssh2_session_free(session);
            closesocket(sock);
            libssh2_exit();
            throw SSHException(error);
        }

        // 认证
        if (libssh2_userauth_password(session, username.c_str(), password.c_str())) {
            string error = "Authentication failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            libssh2_session_disconnect(session, "Auth failed");
            libssh2_session_free(session);
            closesocket(sock);
            libssh2_exit();
            throw SSHException(error);
        }
        
        sessionValid = true;

        // 启动后台监控线程（检测断线并尝试重连）
        if (!monitorRunning.load()) {
            monitorRunning.store(true);
            monitorThread = std::thread([this]() {
                while (monitorRunning.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(monitorIntervalMs));
                    try {
                        if (isSSHDisconnected()) {
                            qDebug() << "监控: 检测到 SSH 断开，尝试重连...";
                            try {
                                reconnect();
                            } catch (const std::exception& e) {
                                qDebug() << "监控: 重连失败:" << e.what();
                            }
                        }
                    } catch (...) {
                        // 忽略监控中的异常，继续下一轮
                    }
                }
            });
        }
    }

    // 检查会话是否仍然有效
    bool checkSessionValidity() {
        if (!session || !sessionValid) return false;
        
        // 尝试执行一个简单的测试命令来检查会话状态
        LIBSSH2_CHANNEL* testChannel = libssh2_channel_open_session(session);
        if (!testChannel) {
            sessionValid = false;
            return false;
        }
        
        libssh2_channel_close(testChannel);
        libssh2_channel_free(testChannel);
        return true;
    }

    // 检查底层 socket 是否已断开（peek 不会从缓冲区移除数据）
    bool checkSocketDisconnected() {
        if (sock == INVALID_SOCKET) return true;
        // 使用 select 检查可读或异常状态（不阻塞）
        fd_set readfds;
        fd_set exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);
        FD_SET(sock, &readfds);
        FD_SET(sock, &exceptfds);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0; // 非阻塞

        int sel = select(0, &readfds, nullptr, &exceptfds, &tv);
        if (sel == SOCKET_ERROR) {
            // select 错误，视作断开
            return true;
        }

        if (sel == 0) {
            // 没有事件，无法判断为断开
            return false;
        }

        // 如果出现异常集合则视为断开
        if (FD_ISSET(sock, &exceptfds)) {
            return true;
        }

        // 如果可读，则 peek 一下
        if (FD_ISSET(sock, &readfds)) {
            char buf;
            int ret = recv(sock, &buf, 1, MSG_PEEK);
            if (ret == 0) {
                // 对端已优雅关闭
                return true;
            } else if (ret < 0) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) {
                    return false;
                }
                return true;
            }
            // 有数据，可认为连接仍然存活
            return false;
        }

        return false;
    }

    // 对外接口：判断 SSH 是否断开（会检查 session 有效性和底层 socket）
    // 返回 true 表示已断开或不可用，false 表示连接看起来还活着
    bool isSSHDisconnected() {
        // 首先快速检查内部标志
        if (!sessionValid || !session) return true;

        // 检查 socket 层面是否断开
        if (checkSocketDisconnected()) {
            sessionValid = false;
            return true;
        }

        // 再次尝试 libssh2 的会话检测（更昂贵但更准确）
        if (!checkSessionValidity()) {
            sessionValid = false;
            return true;
        }

        return false;
    }

    // 重新建立SSH连接
    void reconnect() {
        qDebug() << "尝试重新建立SSH连接...";
        
        // 清理现有资源
        if (session) {
            libssh2_session_disconnect(session, "Reconnecting");
            libssh2_session_free(session);
            session = nullptr;
        }
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        
        // 重新连接
        connectSocket();
        
        // 重新初始化SSH
        session = libssh2_session_init();
        if (!session) {
            closesocket(sock);
            throw SSHException("Failed to recreate SSH session");
        }
        
        libssh2_session_set_blocking(session, 1);
        
        if (libssh2_session_handshake(session, sock)) {
            string error = "SSH rehandshake failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            libssh2_session_free(session);
            closesocket(sock);
            throw SSHException(error);
        }
        
        if (libssh2_userauth_password(session, username.c_str(), password.c_str())) {
            string error = "Reauthentication failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            libssh2_session_disconnect(session, "Reauth failed");
            libssh2_session_free(session);
            closesocket(sock);
            throw SSHException(error);
        }
        
        sessionValid = true;
        qDebug() << "SSH连接重新建立成功";
    }

public:
    SSHManager(const string& host, const string& username, 
               const string& password, int port = 22)
        : host(host), username(username), password(password), port(port) {
        
        // 初始化Winsock
        WSADATA wsadata;
        if (WSAStartup(MAKEWORD(2, 2), &wsadata)) {
            throw SSHException("WSAStartup failed: " + to_string(WSAGetLastError()));
        }
        
        connectSocket();
        initializeSSH();
    }

    ~SSHManager() {
        // 停止监控线程
        monitorRunning.store(false);
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
        if (session) {
            libssh2_session_disconnect(session, "Normal exit");
            libssh2_session_free(session);
        }
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
        libssh2_exit();
        WSACleanup();
    }

    LIBSSH2_SESSION* getSession() { 
        if (!checkSessionValidity()) {
            reconnect();
        }
        return session; 
    }
    
    string getPassword() { return password; }
    
    string getHost() { return host; }
    bool isSessionValid() { return sessionValid; }
    
    // 强制设置会话为无效（用于模拟中断后的状态）
    void invalidateSession() { 
        sessionValid = false; 
    }
};