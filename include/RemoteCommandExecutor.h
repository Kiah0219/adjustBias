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

// 远程命令执行类
class RemoteCommandExecutor {
private:
    LIBSSH2_SESSION* session;
    LIBSSH2_CHANNEL* channel = nullptr;
    string command;
    bool usePTY;
    SSHManager* sshManager; // 用于在需要时重新连接

public:
    LIBSSH2_CHANNEL* getChannel();

    RemoteCommandExecutor(SSHManager* sshManager, const string& command, bool usePTY = true);

    // 执行命令
    void execute();

    // 读取命令输出（支持中断）
    void readOutput();

    ~RemoteCommandExecutor();
};