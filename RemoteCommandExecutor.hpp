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




// 远程命令执行类
class RemoteCommandExecutor {
private:
    LIBSSH2_SESSION* session;
    LIBSSH2_CHANNEL* channel = nullptr;
    string command;
    bool usePTY;
    SSHManager* sshManager; // 用于在需要时重新连接

public:

    // 在RemoteCommandExecutor类的public部分添加：
    LIBSSH2_CHANNEL* getChannel() { return channel; }

    RemoteCommandExecutor(SSHManager* sshManager, const string& command, bool usePTY = true)
        : session(sshManager->getSession()), command(command), usePTY(usePTY), sshManager(sshManager) {
        
        // 打开执行通道
        channel = libssh2_channel_open_session(session);
        if (!channel) {
            string error = "Failed to open execution channel: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            throw SSHException(error + errmsg);
        }

        // 请求伪终端
        if (usePTY && libssh2_channel_request_pty(channel, "xterm")) {
            cerr << "Failed to request pseudo terminal (PTY). Continuing without it." << endl;
        }
    }

    // 执行命令
    void execute() {
        // 执行命令
        if (libssh2_channel_exec(channel, command.c_str())) {
            string error = "Command execution failed: ";
            char* errmsg;
            libssh2_session_last_error(session, &errmsg, nullptr, 0);
            throw SSHException(error + errmsg);
        }
    }

    // 读取命令输出（支持中断）
    void readOutput() {
        // 设置非阻塞模式以便检查中断
        libssh2_session_set_blocking(session, 0);
        
        char outputBuffer[1024];
        int bytesRead;
        bool channelClosed = false;
        
        qDebug() << "脚本输出:";
        
        auto startTime = chrono::steady_clock::now();
        
        while (!channelClosed) {
            // 检查中断标志
            if (g_interrupted) {
                qDebug() << "\n检测到 Ctrl+C，正在向远程进程发送中断信号...";
                
                // 标记SSH管理器会话为无效
                sshManager->invalidateSession();
                
                // 先尝试优雅关闭
                libssh2_channel_send_eof(channel);
                
                // 发送中断信号 (Ctrl+C)
                char ctrl_c = 3; // ASCII 3 是 Ctrl+C
                libssh2_channel_write(channel, &ctrl_c, 1);
                
                // 等待更长时间确保远程进程处理完成
                this_thread::sleep_for(chrono::seconds(2));
                
                // 强制关闭通道
                libssh2_channel_close(channel);
                break;
            }

            // 检查超时（5分钟无活动）
            if (chrono::steady_clock::now() - startTime > chrono::minutes(5)) {
                qDebug() << "\n操作超时，强制结束";
                libssh2_channel_close(channel);
                break;
            }

            // 尝试读取数据
            bytesRead = libssh2_channel_read(channel, outputBuffer, sizeof(outputBuffer));
            
            if (bytesRead == LIBSSH2_ERROR_EAGAIN) {
                // 没有数据，短暂等待后重试
                this_thread::sleep_for(chrono::milliseconds(100));
                continue;
            }
            else if (bytesRead < 0 && bytesRead != LIBSSH2_ERROR_EAGAIN) {
                // 读取错误
                cerr << "从通道读取数据时发生错误" << endl;
                break;
            }
            else if (bytesRead == 0) {
                // 检查通道是否真的关闭了
                if (libssh2_channel_eof(channel)) {
                    channelClosed = true;
                    break;
                }
                // 通道还未关闭，继续等待
                this_thread::sleep_for(chrono::milliseconds(100));
            }
            else {
                // 输出接收到的数据
                qDebug().noquote() << QByteArray(outputBuffer, bytesRead);
                startTime = chrono::steady_clock::now(); // 重置超时计时器
            }
        }
        
        // 确保通道完全关闭
        if (!libssh2_channel_eof(channel)) {
            libssh2_channel_send_eof(channel);
        }
    }

    ~RemoteCommandExecutor() {
        if (channel) {
            // 确保通道完全关闭
            if (!libssh2_channel_eof(channel)) {
                libssh2_channel_send_eof(channel);
            }
            libssh2_channel_close(channel);
            libssh2_channel_free(channel);
        }
    }
};
