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
#include "Exceptions.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

// ===== Backward Compatibility: SSHException alias =====
using SSHException = ApplicationException;

using namespace std;

#include "SSHManager.h"

class FileHandler {
private:
    SSHManager* sshManager;

public:
    FileHandler(SSHManager* manager);

    // 上传文件到远程服务器
    void uploadFile(const string& localPath, const string& remotePath);

    // 删除远程文件（带重连机制）
    void removeRemoteFile(const string& remotePath, int maxRetries = 2);
};