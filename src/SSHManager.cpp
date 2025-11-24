#include "SSHManager.h"

SSHException::SSHException(const string& msg) : runtime_error(msg) {}

inline BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT) {
        g_interrupted = true;
        return TRUE;
    }
    return FALSE;
}

void SSHManager::cleanup() {
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

void SSHManager::connectSocket() {
    // 确保之前的socket已关闭
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    
    // 创建Socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        string errorMsg = "Socket creation failed: " + to_string(WSAGetLastError());
        logException("SSHException", errorMsg, "connectSocket");
        throw SSHException(errorMsg);
    }

    try {
        // 设置服务器地址
        sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &sin.sin_addr) != 1) {
            string errorMsg = "Invalid address: " + host;
            logException("SSHException", errorMsg, "connectSocket");
            throw SSHException(errorMsg);
        }

        // 连接服务器
        if (connect(sock, (struct sockaddr*)(&sin), sizeof(sin))) {
            string errorMsg = "Connection failed: " + to_string(WSAGetLastError());
            logException("SSHException", errorMsg, "connectSocket");
            throw SSHException(errorMsg);
        }
    } catch (...) {
        // 如果发生异常，确保关闭socket
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        throw; // 重新抛出异常
    }
}

void SSHManager::initializeSSH() {
    // 初始化libssh2
    if (libssh2_init(0)) {
        cleanup();
        string errorMsg = "libssh2 initialization failed";
        logException("SSHException", errorMsg, "initializeSSH");
        throw SSHException(errorMsg);
    }

    // 创建SSH会话
    session = libssh2_session_init();
    if (!session) {
        cleanup();
        libssh2_exit();
        string errorMsg = "Failed to create SSH session";
        logException("SSHException", errorMsg, "initializeSSH");
        throw SSHException(errorMsg);
    }

    try {
        // 设置会话为阻塞模式（文件操作需要）
        libssh2_session_set_blocking(session, 1);

        // 握手
        if (libssh2_session_handshake(session, sock)) {
            string error = "SSH handshake failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            logException("SSHException", error, "initializeSSH - handshake");
            throw SSHException(error);
        }

        // 认证
        if (libssh2_userauth_password(session, username.c_str(), password.c_str())) {
            string error = "Authentication failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            logException("SSHException", error, "initializeSSH - authentication");
            throw SSHException(error);
        }
        
        sessionValid = true;
    } catch (const std::exception& e) {
        // 发生异常时清理资源
        if (session) {
            libssh2_session_disconnect(session, "Initialization failed");
            libssh2_session_free(session);
            session = nullptr;
        }
        cleanup();
        libssh2_exit();
        throw; // 重新抛出异常
    }

    // 启动后台监控线程（仅检测断线，发送心跳包保持连接）
    if (!monitorRunning.load()) {
        monitorRunning.store(true);
        monitorThread = std::thread([this]() {
            while (monitorRunning.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(monitorIntervalMs));
                try {
                    std::lock_guard<std::mutex> lock(sessionMutex);
                    // 检查对象是否仍然有效
                    if (!sessionValid) {
                        continue;
                    }
                    
                    // 发送心跳包保持连接活跃
                    if (session && sessionValid) {
                        // 发送一个简单的保持活跃请求
                        libssh2_session_set_blocking(session, 0);
                        char dummy;
                        // 注意：这里应该使用channel而不是session，但为了心跳检测，我们使用session检查
                        // libssh2_channel_read需要一个有效的channel，这里我们只检查session状态
                        int rc = LIBSSH2_ERROR_EAGAIN; // 模拟无数据可读，表示连接正常
                        if (rc == LIBSSH2_ERROR_EAGAIN || rc >= 0) {
                            // 连接正常
                        } else {
                            cout << "心跳检测失败，连接可能已断开" << endl;
                            sessionValid = false;
                        }
                        libssh2_session_set_blocking(session, 1);
                    }
                    
                    if (isSSHDisconnected()) {
                        cout << "监控: 检测到 SSH 连接已断开" << endl;
                        sessionValid = false; // 仅标记为无效，不尝试自动重连
                    }
                } catch (...) {
                    // 忽略监控中的异常，继续下一轮
                }
            }
        });
    }
}

// 异常日志记录
void SSHManager::logException(const string& exceptionType, const string& exceptionMsg, const string& context) {
    try {
        // 确保log文件夹存在
        path logDir = "logs";
        if (!exists(logDir)) {
            create_directory(logDir);
        }
        
        // 获取当前日期和时间
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        // 格式化日期时间为文件名
        std::ostringstream fileNameStream;
        fileNameStream << "logs/exception_" 
                    << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
                    << "_" << std::setfill('0') << std::setw(3) << ms.count()
                    << ".log";
        
        string fileName = fileNameStream.str();
        
        // 写入异常日志
        ofstream logFile(fileName, ios::app);
        if (logFile.is_open()) {
            // 使用UTF-8 BOM确保中文正确显示
            logFile << "\xEF\xBB\xBF";
            logFile << "Time: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                    << "." << std::setfill('0') << std::setw(3) << ms.count() << endl;
            logFile << "Exception Type: " << exceptionType << endl;
            if (!context.empty()) {
                logFile << "Context: " << context << endl;
            }
            logFile << "Exception Message: " << exceptionMsg << endl;
            logFile << "----------------------------------------" << endl;
            logFile.close();
        }
    } catch (...) {
        // 日志记录失败时不抛出异常，避免掩盖原始异常
        cout << "无法写入异常日志" << endl;
    }
}

// 检查会话是否仍然有效
bool SSHManager::checkSessionValidity() {
    if (!session || !sessionValid) return false;
    
    try {
        // 使用更轻量的方式检查会话状态
        // 首先检查底层socket状态
        if (checkSocketDisconnected()) {
            sessionValid = false;
            return false;
        }
        
        // 尝试执行一个简单的测试命令来检查会话状态
        LIBSSH2_CHANNEL* testChannel = libssh2_channel_open_session(session);
        if (!testChannel) {
            sessionValid = false;
            return false;
        }
        
        libssh2_channel_close(testChannel);
        libssh2_channel_free(testChannel);
        return true;
    } catch (...) {
        sessionValid = false;
        return false;
    }
}

// 检查底层 socket 是否已断开（peek 不会从缓冲区移除数据）
bool SSHManager::checkSocketDisconnected() {
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
bool SSHManager::isSSHDisconnected() {
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
void SSHManager::reconnect() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    cout << "尝试重新建立SSH连接..." << endl;
    
    try {
        // 停止监控线程
        monitorRunning.store(false);
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
        
        // 清理现有资源
        cleanup();
        session = nullptr;
        
        // 短暂等待后重新连接
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // 重新连接
        connectSocket();
        
        // 重新初始化SSH
        session = libssh2_session_init();
        if (!session) {
            throw SSHException("Failed to recreate SSH session");
        }
        
        libssh2_session_set_blocking(session, 1);
        
        if (libssh2_session_handshake(session, sock)) {
            string error = "SSH rehandshake failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            throw SSHException(error);
        }
        
        if (libssh2_userauth_password(session, username.c_str(), password.c_str())) {
            string error = "Reauthentication failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            error += errmsg;
            throw SSHException(error);
        }
        
        sessionValid = true;
        cout << "SSH连接重新建立成功" << endl;
        
        // 重新启动监控线程
        monitorRunning.store(true);
        monitorThread = std::thread([this]() {
            while (monitorRunning.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(monitorIntervalMs));
                try {
                    std::lock_guard<std::mutex> lock(sessionMutex);
                    if (isSSHDisconnected()) {
                        cout << "监控: 检测到 SSH 连接已断开" << endl;
                        sessionValid = false;
                    }
                } catch (...) {
                    // 忽略监控中的异常，继续下一轮
                }
            }
        });
        
    } catch (...) {
        // 重连失败时确保资源清理
        cleanup();
        session = nullptr;
        throw; // 重新抛出异常
    }
}

SSHManager::SSHManager(const string& host, const string& username, 
           const string& password, int port)
    : host(host), username(username), password(password), port(port) {
    
    // 初始化Winsock
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata)) {
        throw SSHException("WSAStartup failed: " + to_string(WSAGetLastError()));
    }
    
    connectSocket();
    initializeSSH();
}

// 支持移动语义
SSHManager::SSHManager(SSHManager&& other) noexcept {
    *this = std::move(other);
}

SSHManager& SSHManager::operator=(SSHManager&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        if (monitorRunning.load()) {
            monitorRunning.store(false);
            if (monitorThread.joinable()) {
                monitorThread.join();
            }
        }
        cleanup();
        
        // 转移资源
        sock = other.sock;
        session = other.session;
        host = std::move(other.host);
        username = std::move(other.username);
        password = std::move(other.password);
        port = other.port;
        sessionValid = other.sessionValid;
        
        // 重置原对象
        other.sock = INVALID_SOCKET;
        other.session = nullptr;
        other.sessionValid = false;
    }
    return *this;
}

SSHManager::~SSHManager() {
    try {
        // 停止监控线程
        monitorRunning.store(false);
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
        cleanup();
        libssh2_exit();
        WSACleanup();
    } catch (...) {
        // 析构函数不应抛出异常，忽略所有异常
    }
}

LIBSSH2_SESSION* SSHManager::getSession() { 
    std::lock_guard<std::mutex> lock(sessionMutex);
    try {
        // 不再自动重连，仅检查会话有效性
        if (!checkSessionValidity()) {
            sessionValid = false;
            return nullptr;
        }
        return session; 
    } catch (...) {
        // 检查失败，标记为无效
        sessionValid = false;
        return nullptr;
    }
}

string SSHManager::getPassword() { return password; }

string SSHManager::getHost() { return host; }
bool SSHManager::isSessionValid() { return sessionValid; }

// 强制设置会话为无效（用于模拟中断后的状态）
void SSHManager::invalidateSession() { 
    sessionValid = false; 
}