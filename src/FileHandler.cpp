#include "FileHandler.h"
#include <QDebug>

FileHandler::FileHandler(SSHManager* manager) : sshManager(manager) {}

void FileHandler::uploadFile(const string& localPath, const string& remotePath) {
    LIBSSH2_SESSION* session = sshManager->getSession();
    if (!session) {
        throw SSHException("无法获取有效的SSH会话，无法上传文件");
    }

    // 打开本地文件
    ifstream file(localPath, ios::binary | ios::ate);
    if (!file.is_open()) {
        throw SSHException("Failed to open local file: " + localPath);
    }

    // 获取文件大小
    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg);

    // 创建SCP通道
    LIBSSH2_CHANNEL* scpChannel = libssh2_scp_send(
        session, 
        remotePath.c_str(), 
        0700,
        static_cast<size_t>(fileSize)
    );

    if (!scpChannel) {
        string error = "SCP channel creation failed: ";
        char* errmsg;
        libssh2_session_last_error(session, &errmsg, nullptr, 0);
        error += errmsg;
        file.close();
        throw SSHException(error);
    }

    // 上传文件内容
    vector<char> buffer(1024);
    while (!file.eof() && !g_interrupted) {
        file.read(buffer.data(), buffer.size());
        int bytesRead = static_cast<int>(file.gcount());
        
        if (libssh2_channel_write(scpChannel, buffer.data(), bytesRead) != bytesRead) {
            file.close();
            libssh2_channel_send_eof(scpChannel);
            libssh2_channel_close(scpChannel);
            libssh2_channel_free(scpChannel);
            throw SSHException("File upload failed");
        }
    }
    file.close();
    
    // 关闭SCP通道
    libssh2_channel_send_eof(scpChannel);
    libssh2_channel_close(scpChannel);
    libssh2_channel_free(scpChannel);
}

void FileHandler::removeRemoteFile(const string& remotePath, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            LIBSSH2_SESSION* session = sshManager->getSession();
            if (!session) {
                if (attempt < maxRetries - 1) {
                    qDebug() << "无法获取有效SSH会话，尝试重连 (" << (attempt + 1) << "/" << maxRetries << ")...";
                    continue;
                } else {
                    throw SSHException("无法获取有效SSH会话，删除文件失败");
                }
            }

            LIBSSH2_CHANNEL* rmChannel = libssh2_channel_open_session(session);
            if (!rmChannel) {
                if (attempt < maxRetries - 1) {
                    qDebug() << "删除通道打开失败，尝试重新连接 (" << (attempt + 1) << "/" << maxRetries << ")...";
                    continue;
                } else {
                    throw SSHException("Failed to open channel for rm command after " + to_string(maxRetries) + " attempts");
                }
            }

            string command = "rm -f " + remotePath;
            if (libssh2_channel_exec(rmChannel, command.c_str()) == 0) {
                // 等待命令执行完成
                char buffer[1024];
                int bytesRead;
                do {
                    bytesRead = libssh2_channel_read(rmChannel, buffer, sizeof(buffer));
                } while (bytesRead > 0);
                
                // 检查退出状态
                int exitCode = libssh2_channel_get_exit_status(rmChannel);
                libssh2_channel_close(rmChannel);
                libssh2_channel_free(rmChannel);
                
                if (exitCode == 0) {
                    qDebug() << "远程文件 " << remotePath << " 删除成功";
                    return;
                } else {
                    throw SSHException("Failed to delete remote file. Exit code: " + to_string(exitCode));
                }
            } else {
                libssh2_channel_close(rmChannel);
                libssh2_channel_free(rmChannel);
                throw SSHException("Failed to execute rm command");
            }
        } catch (const SSHException& e) {
            if (attempt == maxRetries - 1) {
                throw; // 最后一次尝试仍然失败，重新抛出异常
            }
            qDebug() << "删除尝试 " << (attempt + 1) << " 失败: " << e.what();
            this_thread::sleep_for(chrono::seconds(1)); // 等待1秒后重试
        }
    }
}