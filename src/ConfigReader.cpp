 
#include "ConfigReader.h"
#include <QDebug>
#include <unordered_map>

// base64 encoder helper
static std::string base64_encode(const std::string &in) {
    static const char *b64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

// 确保字符串以单个换行符结尾（如果字符串非空），用于保证文件最后一行保留换行符
static void ensure_single_trailing_newline(std::string &s) {
    if (s.empty()) return;
    while (!s.empty() && s.back() == '\n') s.pop_back();
    s.push_back('\n');
}

// ===== Optimization #3: Initialize parameter map for O(1) lookup =====
void ConfigReader::initializeParameterMap() {
    parameterMap = {
        {"xsense_data_roll", &xsense_data_roll},
        {"xsense_data_pitch", &xsense_data_pitch},
        {"x_vel_offset", &x_vel_offset},
        {"y_vel_offset", &y_vel_offset},
        {"yaw_vel_offset", &yaw_vel_offset},
        {"x_vel_offset_run", &x_vel_offset_run},
        {"y_vel_offset_run", &y_vel_offset_run},
        {"yaw_vel_offset_run", &yaw_vel_offset_run},
        {"x_vel_limit_walk", &x_vel_limit_walk},
        {"x_vel_limit_run", &x_vel_limit_run}
    };
}

bool ConfigReader::validateConfigFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    // 简单验证文件内容是否为空
    std::string line;
    std::getline(file, line);
    return !line.empty();
}
        
bool ConfigReader::atomicWriteRemoteFile(const std::string& content) {
    if (!sshManager) {
        std::cerr << "错误: SSH管理器未初始化，无法写入文件" << std::endl;
        return false;
    }

    if (sshManager->isSSHDisconnected()) {
        std::cerr << "错误: SSH连接已断开，无法写入文件" << std::endl;
        return false;
    }

    std::string tmpPath; // 定义在外部以便 catch 块使用
    try {

        // 在相同目录下生成临时文件路径
        tmpPath = configPath + ".tmp." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        // 使用 base64 方式写入，避免额外换行和 heredoc 问题
        std::string toWrite = content;
        ensure_single_trailing_newline(toWrite); // 保证末尾有且仅有一个换行
        std::string encoded = base64_encode(toWrite);
        // 先检测远端是否存在 base64 工具，若无则回退到 heredoc（可能面临 EOF 关键词问题）
        std::string checkBase64Cmd = "command -v base64 >/dev/null 2>&1 && echo '1' || echo '0'";
        std::string hasBase64 = executeRemoteCommand(checkBase64Cmd);
        std::string writeTempCmd;
        if (hasBase64.find('1') != std::string::npos) {
            writeTempCmd = "echo '" + encoded + "' | base64 -d > " + tmpPath;
        } else {
            // fallback to heredoc; escape single quotes by closing and reopening quoting
            // But heredoc is susceptible to EOF content - we try to avoid extra ending EOF text by ensuring newline
            writeTempCmd = "cat > " + tmpPath + " << 'EOF'\n" + toWrite + "\nEOF";
        }
        std::string writeResult = executeRemoteCommand(writeTempCmd);
        Q_UNUSED(writeResult);

        // 验证临时文件是否存在
        std::string testTmpCmd = "test -f " + tmpPath + " && echo '1' || echo '0'";
        std::string tmpExists = executeRemoteCommand(testTmpCmd);
        if (tmpExists.find('1') == std::string::npos) {
            // 临时文件写入失败
            cerr << "临时文件未创建: " << tmpPath << std::endl;
            // 尝试删除临时文件（防止残留）
            executeRemoteCommand(std::string("rm -f ") + tmpPath);
            return false;
        }

        // 将临时文件移动到目标路径（覆盖）
        std::string mvCmd = "mv -f " + tmpPath + " " + configPath;
        try {
            std::string mvResult = executeRemoteCommand(mvCmd);
            Q_UNUSED(mvResult);
        } catch (...) {
            // mv 失败，尝试删除临时文件以免泄露
            try {
                executeRemoteCommand(std::string("rm -f ") + tmpPath);
            } catch (...) {
                // 如果删除也失败，记录并返回失败
            }
            throw; // 重新抛出以由外层处理
        }

        // 成功移动，临时文件已替换为目标文件（不会残留tmp）
        return true;
    } catch (const SSHException& e) {
        std::cerr << "SSH异常: 写入远端文件失败: " << e.what() << std::endl;
        // 尝试删除残留临时文件
        try { if (!tmpPath.empty()) executeRemoteCommand(std::string("rm -f ") + tmpPath); } catch (...) {}
        return false;
    } catch (const std::exception& e) {
        std::cerr << "写入远端文件失败: " << e.what() << std::endl;
        try { if (!tmpPath.empty()) executeRemoteCommand(std::string("rm -f ") + tmpPath); } catch (...) {}
        return false;
    } catch (...) {
        std::cerr << "未知异常: 写入远端文件失败" << std::endl;
        try { if (!tmpPath.empty()) executeRemoteCommand(std::string("rm -f ") + tmpPath); } catch (...) {}
        return false;
    }
}

// ===== Optimization #3: Use map-based lookup instead of if-else chain =====
// Performance improvement: O(n) -> O(1) parameter lookup
void ConfigReader::setParameterValue(const std::string& varName, double value) {
    auto it = parameterMap.find(varName);
    if (it != parameterMap.end()) {
        *(it->second) = value;  // Direct pointer assignment - O(1) lookup
    }
}

ConfigReader::ConfigReader(SSHManager* manager, const std::string& configPath) 
    : sshManager(manager), configPath(configPath) {
    initializeParameterMap();  // Initialize parameter map for O(1) lookups
}

std::string ConfigReader::executeRemoteCommand(const std::string& command, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            // 检查SSH连接状态
            if (!sshManager || sshManager->isSSHDisconnected()) {
                if (attempt < maxRetries - 1) {
                    qDebug() << "SSH连接断开，尝试重连 (" << (attempt + 1) << "/" << maxRetries << ")...";
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    continue;
                } else {
                    throw SSHException("SSH连接断开且重连失败");
                }
            }
            
            // 获取SSH会话并检查有效性
            LIBSSH2_SESSION* session = sshManager->getSession();
            if (!session) {
                if (attempt < maxRetries - 1) {
                    qDebug() << "无法获取有效SSH会话，尝试重连 (" << (attempt + 1) << "/" << maxRetries << ")...";
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    continue;
                } else {
                    throw SSHException("无法获取有效SSH会话");
                }
            }
            
            RemoteCommandExecutor executor(sshManager, command, false);
            executor.execute();

            // 为了可靠读取完整输出，使用阻塞模式并读取直到通道 EOF
            libssh2_session_set_blocking(sshManager->getSession(), 1);
            char buffer[1024];
            std::string result;

            auto startTime = chrono::steady_clock::now();
            auto timeout = chrono::seconds(30); // 30秒超时

            while (true) {
                // 检查超时
                if (chrono::steady_clock::now() - startTime > timeout) {
                    qDebug() << "命令执行超时: " << QString::fromStdString(command);
                    break;
                }
                
                int bytesRead = libssh2_channel_read(executor.getChannel(), buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    result.append(buffer, bytesRead);
                    startTime = chrono::steady_clock::now(); // 重置超时计时器
                    continue;
                } else if (bytesRead == 0) {
                    // 如果通道已经到 EOF，读取完成
                    if (libssh2_channel_eof(executor.getChannel())) {
                        break;
                    }
                    // 否则短暂等待再读
                    this_thread::sleep_for(chrono::milliseconds(50));
                    continue;
                } else if (bytesRead == LIBSSH2_ERROR_EAGAIN) {
                    // 非阻塞提示（理论上不会出现，因为已设置阻塞），短等重试
                    this_thread::sleep_for(chrono::milliseconds(50));
                    continue;
                } else {
                    // 出现错误，返回当前已读内容（并由调用方判断是否为空）
                    break;
                }
            }

            return result;
            
        } catch (const SSHException& e) {
            if (attempt == maxRetries - 1) {
                throw; // 最后一次尝试仍然失败，重新抛出异常
            }
            qDebug() << "命令执行尝试 " << (attempt + 1) << " 失败: " << e.what();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        } catch (const std::exception& e) {
            if (attempt == maxRetries - 1) {
                throw SSHException(std::string("执行命令时发生异常: ") + e.what());
            }
            qDebug() << "命令执行异常 " << (attempt + 1) << ": " << e.what();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    return ""; // 所有尝试都失败
}

bool ConfigReader::createDefaultConfig() {
    try {
        // 检查sshManager是否有效
        if (!sshManager) {
            std::cerr << "错误: SSH管理器未初始化，无法创建默认配置文件" << std::endl;
            return false;
        }
        
        qDebug() << "正在创建默认配置文件: " << configPath;
        
        // 创建目录（如果不存在）
        size_t lastSlash = configPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string dirPath = configPath.substr(0, lastSlash);
            std::string mkdirCommand = "mkdir -p " + dirPath;
            std::string result = executeRemoteCommand(mkdirCommand);
            if (result.find("error") != string::npos) {
                cerr << "创建目录失败: " << dirPath << endl;
            }
        }
        
        // 默认配置文件内容
        string defaultConfig = 
            "xsense_data_roll=0.0\n"
            "xsense_data_pitch=0.0\n"
            "x_vel_offset=0.0\n"
            "y_vel_offset=0.0\n"
            "yaw_vel_offset=0.0\n"
            "x_vel_offset_run=0.0\n"
            "y_vel_offset_run=0.0\n"
            "yaw_vel_offset_run=0.0\n"
            ;
        
        // 创建配置文件（原子写入）
        bool createOk = atomicWriteRemoteFile(defaultConfig);
        if (!createOk) {
            cerr << "配置文件创建失败: 写入默认配置失败" << endl;
            return false;
        }
        
        // 检查文件是否创建成功
        string checkCommand = "test -f " + configPath + " && echo \"created\" || echo \"failed\"";
        string createCheckResult = executeRemoteCommand(checkCommand);

        if (createCheckResult.find("created") != string::npos) {
            qDebug() << "默认配置文件创建成功: " << configPath;
            return true;
        } else {
            cerr << "配置文件创建失败: " << configPath << endl;
            return false;
        }
        
    } catch (const SSHException& e) {
        cerr << "SSH异常: 创建配置文件时发生错误: " << e.what() << endl;
        return false;
    } catch (const exception& e) {
        cerr << "创建配置文件时发生错误: " << e.what() << endl;
        return false;
    } catch (...) {
        cerr << "未知异常: 创建配置文件时发生错误" << endl;
        return false;
    }
}

bool ConfigReader::parseConfigContent(const std::string& content, std::string &dedupedContent) {
    parsedParams.clear(); // 清空已解析参数集合

    // 保存原始每一行，用以重建去重后的内容
    std::vector<std::string> originalLines;
    std::istringstream contentStream(content);
    std::string line;
    while (std::getline(contentStream, line)) {
        originalLines.push_back(line);
    }

    // 存储参数的最后一次出现行索引和对应的数值
    std::unordered_map<std::string, size_t> lastOccurrenceIndex;
    std::unordered_map<std::string, double> lastParsedValues;

    // 解析并记录最后一次出现的值
    for (size_t i = 0; i < originalLines.size(); ++i) {
        std::string current = originalLines[i];
        // 去除行首尾空白字符
        auto start = current.find_first_not_of(" \t");
        if (start == std::string::npos) continue; // 空白行
        std::string trimmed = current.substr(start);
        // 跳过注释行
        if (trimmed.empty() || trimmed[0] == '#') continue;
        
        size_t equalPos = trimmed.find('=');
        if (equalPos == std::string::npos) continue;

        std::string varName = trimmed.substr(0, equalPos);
        std::string valueStr = trimmed.substr(equalPos + 1);

        // 去除变量名和值的空白字符
        varName.erase(0, varName.find_first_not_of(" \t"));
        varName.erase(varName.find_last_not_of(" \t") + 1);
        valueStr.erase(0, valueStr.find_first_not_of(" \t"));
        valueStr.erase(valueStr.find_last_not_of(" \t") + 1);

        try {
            // 验证数值字符串格式
            if (valueStr.empty() || valueStr.find_first_not_of("0123456789.-+eE") != std::string::npos) {
                // 非数值样式，不处理
                continue;
            }
            double value = std::stod(valueStr);
            if (!std::isfinite(value)) continue;

            // 更新最后一次出现记录
            lastOccurrenceIndex[varName] = i;
            lastParsedValues[varName] = value;
        } catch (...) {
            // 忽略解析中出现的异常，继续下一行
            continue;
        }
    }

    // 将最终值写入内存参数，并标记为已解析
    for (const auto& kv : lastParsedValues) {
        setParameterValue(kv.first, kv.second);
        parsedParams.insert(kv.first);
    }

    // 构建去重后的文件内容（仅保留每个参数的最后一次出现）
    std::vector<std::string> dedupedLines;
    dedupedLines.reserve(originalLines.size());
    for (size_t i = 0; i < originalLines.size(); ++i) {
        std::string current = originalLines[i];
        auto start = current.find_first_not_of(" \t");
        if (start == std::string::npos) {
            dedupedLines.push_back(current);
            continue; // 空行
        }
        std::string trimmed = current.substr(start);
        if (trimmed.empty() || trimmed[0] == '#') {
            dedupedLines.push_back(current);
            continue; // 注释行保留
        }

        size_t equalPos = trimmed.find('=');
        if (equalPos == std::string::npos) {
            dedupedLines.push_back(current);
            continue; // 非键值行保留
        }

        std::string varName = trimmed.substr(0, equalPos);
        varName.erase(0, varName.find_first_not_of(" \t"));
        varName.erase(varName.find_last_not_of(" \t") + 1);

        auto it = lastOccurrenceIndex.find(varName);
        if (it != lastOccurrenceIndex.end() && it->second != i) {
            // 不是最后一次出现，跳过（删除）
            continue;
        }

        // 留下最后一次或非参数行
        dedupedLines.push_back(current);
    }

    // ===== 优化: 去除末尾重复空行，最多保留一行空行 =====
    auto isBlank = [](const std::string &s) {
        if (s.empty()) return true;
        size_t st = s.find_first_not_of(" \t\r\n");
        return st == std::string::npos;
    };
    // 删除末尾的空行，避免文件结尾出现多余空行（保留0个）
    while (!dedupedLines.empty() && isBlank(dedupedLines.back())) {
        dedupedLines.pop_back();
    }

    // 将 dedupedLines 合并为字符串
    std::ostringstream oss;
    for (size_t i = 0; i < dedupedLines.size(); ++i) {
        oss << dedupedLines[i];
        if (i + 1 < dedupedLines.size()) oss << '\n';
    }
    dedupedContent = oss.str();
    if(!dedupedContent.empty()) {
        // 保证去重后文件在末尾保留一个换行符
        dedupedContent.push_back('\n');
    }
    return true;
}

bool ConfigReader::completeMissingParameters() {
    try {
        // 检查sshManager是否有效
        if (!sshManager) {
            cerr << "错误: SSH管理器未初始化，无法补充缺失参数" << endl;
            return false;
        }
        vector<string> missingParams;
        // 跳过 x_vel_limit_walk 和 x_vel_limit_run
        for (const auto& param : expectedParams) {
            if (param == "x_vel_limit_walk" || param == "x_vel_limit_run") {                    
                continue;
            }
            if (parsedParams.find(param) == parsedParams.end()) {
                missingParams.push_back(param);
            }
        }

        if (missingParams.empty()) {
            qDebug() << "配置文件已包含所有预期参数，无需补充。";
            return true;
        }
        
        qDebug() << "发现缺失参数，正在补充: ";
        for (const auto& param : missingParams) {
            qDebug() << param << " ";
        }
        qDebug();
        
        // 读取当前配置文件内容
        string readCommand = "cat " + configPath;
        string fileContent = executeRemoteCommand(readCommand);
        
        // 在文件末尾添加缺失参数
        stringstream newContent;
        newContent << fileContent;
        
        // 如果原文件末尾没有换行，先添加一个换行
        if (!fileContent.empty() && fileContent.back() != '\n') {
            newContent << "\n";
        }
        
        // newContent << "\n# 自动补充的缺失参数\n";
        for (const auto& param : missingParams) {
            newContent << param << "=0.0\n";
            setParameterValue(param, 0.0); // 设置默认值
            parsedParams.insert(param);    // 标记为已解析
        }
        
        // 原子写回文件（写临时文件并 mv 覆盖）
        bool writeOk = atomicWriteRemoteFile(newContent.str());
        if (!writeOk) {
            cerr << "写回缺失参数到文件失败" << endl;
            return false;
        }
        
        qDebug() << "已成功补充 " << missingParams.size() << " 个缺失参数到配置文件。";
        return true;
        
    } catch (const SSHException& e) {
        cerr << "SSH异常: 补充缺失参数失败: " << e.what() << endl;
        return false;
    } catch (const exception& e) {
        cerr << "补充缺失参数失败: " << e.what() << endl;
        return false;
    } catch (...) {
        cerr << "未知异常: 补充缺失参数失败" << endl;
        return false;
    }
}

bool ConfigReader::writeParameterToFile(const std::string& paramName, double value) {
    try {
        // 检查参数名是否有效
        if (paramName.empty()) {
            cerr << "错误: 参数名不能为空" << endl;
            return false;
        }
        
        // 检查sshManager是否有效
        if (!sshManager) {
            cerr << "错误: SSH管理器未初始化" << endl;
            return false;
        }
        
        // 检查SSH连接状态
        if (sshManager->isSSHDisconnected()) {
            cerr << "错误: SSH连接已断开，无法写入配置文件" << endl;
            return false;
        }
        
        // 读取当前配置文件内容
        string readCommand = "cat " + configPath;
        string fileContent = executeRemoteCommand(readCommand);
        
        if (fileContent.empty()) {
            cerr << "配置文件内容为空或读取失败" << endl;
            return false;
        }
        
        // 将内容按行分割
        vector<string> lines;
        istringstream contentStream(fileContent);
        string line;
        bool paramFound = false;
        
        while (getline(contentStream, line)) {
            // 查找目标参数行
            string trimmedLine = line;
            trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));
            
            if (trimmedLine.find(paramName + "=") == 0) {
                // 找到参数行，更新值
                stringstream newLine;
                newLine << paramName << "=" << value;
                lines.push_back(newLine.str());
                paramFound = true;
            } else {
                lines.push_back(line);
            }
        }
        
        // 如果参数不存在，添加到文件末尾
        if (!paramFound) {
            stringstream newLine;
            newLine << paramName << "=" << value;
            lines.push_back(newLine.str());
            parsedParams.insert(paramName); // 添加到已解析集合
        }

        
        // 显示最终要写入的内容
        qDebug() << "最终要写入的配置文件内容:";
        string finalContent;
        for (const auto& currentLine : lines) {
            finalContent += currentLine + "\n";
            qDebug().noquote() << QString::fromStdString(currentLine);
        }
        qDebug() << "最终配置文件行数:" << lines.size();
        
        // 原子写回文件
        bool writeOk = atomicWriteRemoteFile(finalContent);
        if (!writeOk) {
            cerr << "写入参数到文件失败" << endl;
            return false;
        }
        
        // 更新内存中的参数值
        setParameterValue(paramName, value);
        
        qDebug() << "参数 " << paramName << " 已成功写入配置文件: " << value;
        return true;
        
    } catch (const SSHException& e) {
        cerr << "SSH异常: 写入参数到文件失败: " << paramName << " - " << e.what() << endl;
        return false;
    } catch (const exception& e) {
        cerr << "写入参数到文件失败: " << paramName << " - " << e.what() << endl;
        return false;
    } catch (...) {
        cerr << "未知异常: 写入参数到文件失败: " << paramName << endl;
        return false;
    }
}

bool ConfigReader::loadConfig() {
    try {
        // 检查sshManager是否有效
        if (!sshManager) {
            cerr << "错误: SSH管理器未初始化" << endl;
            return false;
        }
        
        // 检查SSH连接状态
        if (sshManager->isSSHDisconnected()) {
            cerr << "错误: SSH连接已断开，无法加载配置文件" << endl;
            return false;
        }
        
        qDebug() << "正在检查远程配置文件: " << configPath;
        
        // 检查文件是否存在
        string checkCommand = "test -f " + configPath + " && echo \"existed\" || echo \"not_exist\"";
        string result = executeRemoteCommand(checkCommand);
        
        if (result.find("existed") != string::npos) {
            qDebug() << "配置文件存在，开始读取和验证...";
            
            // 读取文件内容
            string readCommand = "cat " + configPath;
            string fileContent = executeRemoteCommand(readCommand);
            
            qDebug() << "配置文件原始内容:";
            qDebug().noquote() << fileContent;
            
            // 解析文件内容，并去重重复参数（保留最后一次出现）
            string dedupedContent;
            bool parsedOk = parseConfigContent(fileContent, dedupedContent);
            if (!parsedOk) {
                cerr << "解析配置文件失败" << endl;
                // 继续仍然尝试补充缺失参数
            }
            // 如果发现重复参数并且去重后的内容不同，则写回去重版本
            if (!dedupedContent.empty() && dedupedContent != fileContent) {
                qDebug() << "检测到重复参数，正在写回去重后的配置文件...";
                bool writeOk = atomicWriteRemoteFile(dedupedContent);
                if (!writeOk) {
                    cerr << "写回去重后的配置文件失败" << endl;
                }
            }
            
            // 检查并补充缺失参数
            if (completeMissingParameters()) {
                qDebug() << "配置文件验证和补充完成!";
            } else {
                cerr << "配置文件补充失败，但将继续使用现有参数" << endl;
            }
            
            configLoaded = true;
            return true;
            
        } else {
            qDebug() << "配置文件不存在，尝试创建默认配置文件...";
            
            // 创建默认配置文件
            if (createDefaultConfig()) {
                // 重新读取新创建的配置文件
                string readCommand = "cat " + configPath;
                string fileContent = executeRemoteCommand(readCommand);
                
                qDebug() << "新创建的配置文件内容:";
                qDebug().noquote() << fileContent;
                
                // 解析文件内容（虽然默认配置不会重复，但仍使用带去重的解析器）
                string dedupedContent;
                bool parsedOk = parseConfigContent(fileContent, dedupedContent);
                if (!parsedOk) {
                    cerr << "解析默认配置文件失败" << endl;
                }
                
                // 如果去重后与原始不同，写回以保持一致（通常不会发生）
                if (!dedupedContent.empty() && dedupedContent != fileContent) {
                    qDebug() << "默认配置文件去重后发生改变，正在写回...";
                    bool writeOk = atomicWriteRemoteFile(dedupedContent);
                    if (!writeOk) {
                        cerr << "写回默认配置文件失败" << endl;
                    }
                }
                qDebug() << "默认配置文件创建并解析完成!";
                configLoaded = true;
                return true;
            } else {
                cerr << "无法创建配置文件，使用默认参数值0.0" << endl;
                configLoaded = false;
                return false;
            }
        }
        
    } catch (const SSHException& e) {
        cerr << "SSH异常: 加载配置文件失败: " << e.what() << endl;
        configLoaded = false;
        return false;
    } catch (const exception& e) {
        cerr << "加载配置文件失败: " << e.what() << endl;
        configLoaded = false;
        return false;
    } catch (...) {
        cerr << "未知异常: 加载配置文件失败" << endl;
        configLoaded = false;
        return false;
    }
}

void ConfigReader::writeAllValuesToFile(){
    // 批量写入所有参数，避免多次SSH操作
    vector<pair<string, double>> params = {
        {"xsense_data_roll", this->xsense_data_roll},
        {"xsense_data_pitch", this->xsense_data_pitch},
        {"x_vel_offset", this->x_vel_offset},
        {"y_vel_offset", this->y_vel_offset},
        {"yaw_vel_offset", this->yaw_vel_offset},
        {"x_vel_offset_run", this->x_vel_offset_run},
        {"y_vel_offset_run", this->y_vel_offset_run},
        {"yaw_vel_offset_run", this->yaw_vel_offset_run},
        {"x_vel_limit_walk", this->x_vel_limit_walk},
        {"x_vel_limit_run", this->x_vel_limit_run}
    };
    
    writeMultipleParametersToFile(params);
}

bool ConfigReader::updateMultipleParameters(
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
) {
    try {
        // 检查sshManager是否有效
        if (!sshManager) {
            cerr << "错误: SSH管理器未初始化" << endl;
            return false;
        }
        
        // 检查SSH连接状态
        if (sshManager->isSSHDisconnected()) {
            cerr << "错误: SSH连接已断开，无法写入配置文件" << endl;
            return false;
        }
        
        // 批量更新所有参数，避免多次SSH操作
        vector<pair<string, double>> params = {
            {"xsense_data_roll", xsense_data_roll},
            {"xsense_data_pitch", xsense_data_pitch},
            {"x_vel_offset", x_vel_offset},
            {"y_vel_offset", y_vel_offset},
            {"yaw_vel_offset", yaw_vel_offset},
            {"x_vel_offset_run", x_vel_offset_run},
            {"y_vel_offset_run", y_vel_offset_run},
            {"yaw_vel_offset_run", yaw_vel_offset_run},
            {"x_vel_limit_walk", x_vel_limit_walk},
            {"x_vel_limit_run", x_vel_limit_run}
        };
        
        // 更新内存中的参数值
        this->xsense_data_roll = xsense_data_roll;
        this->xsense_data_pitch = xsense_data_pitch;
        this->x_vel_offset = x_vel_offset;
        this->y_vel_offset = y_vel_offset;
        this->yaw_vel_offset = yaw_vel_offset;
        this->x_vel_offset_run = x_vel_offset_run;
        this->y_vel_offset_run = y_vel_offset_run;
        this->yaw_vel_offset_run = yaw_vel_offset_run;
        this->x_vel_limit_walk = x_vel_limit_walk;
        this->x_vel_limit_run = x_vel_limit_run;
        
        // 使用批量写入方法
        bool success = writeMultipleParametersToFile(params);
        
        if (success) {
            qDebug() << "批量参数更新成功!";
        }
        
        return success;
        
    } catch (const SSHException& e) {
        cerr << "SSH异常: 批量更新参数失败: " << e.what() << endl;
        return false;
    } catch (const exception& e) {
        cerr << "批量更新参数失败: " << e.what() << endl;
        return false;
    } catch (...) {
        cerr << "未知异常: 批量更新参数失败" << endl;
        return false;
    }
}

// 获取参数值的方法
double ConfigReader::getXsenseDataRoll() const { return xsense_data_roll; }
double ConfigReader::getXsenseDataPitch() const { return xsense_data_pitch; }
double ConfigReader::getXVelOffset() const { return x_vel_offset; }
double ConfigReader::getYVelOffset() const { return y_vel_offset; }
double ConfigReader::getYawVelOffset() const { return yaw_vel_offset; }
double ConfigReader::getXVelOffsetRun() const { return x_vel_offset_run; }
double ConfigReader::getYVelOffsetRun() const { return y_vel_offset_run; }
double ConfigReader::getYawVelOffsetRun() const { return yaw_vel_offset_run; }
double ConfigReader::getXVelLimitWalk() const { return x_vel_limit_walk; }
double ConfigReader::getXVelLimitRun() const { return x_vel_limit_run; }

// 设置参数值并写入文件
bool ConfigReader::setXsenseDataRoll(double value) { 
    return writeParameterToFile("xsense_data_roll", value); 
}
bool ConfigReader::setXsenseDataPitch(double value) { 
    return writeParameterToFile("xsense_data_pitch", value); 
}
bool ConfigReader::setXVelOffset(double value) { 
    return writeParameterToFile("x_vel_offset", value); 
}
bool ConfigReader::setYVelOffset(double value) { 
    return writeParameterToFile("y_vel_offset", value); 
}
bool ConfigReader::setYawVelOffset(double value) { 
    return writeParameterToFile("yaw_vel_offset", value); 
}
bool ConfigReader::setXVelOffsetRun(double value) { 
    return writeParameterToFile("x_vel_offset_run", value); 
}
bool ConfigReader::setYVelOffsetRun(double value) { 
    return writeParameterToFile("y_vel_offset_run", value); 
}
bool ConfigReader::setYawVelOffsetRun(double value) {
    return writeParameterToFile("yaw_vel_offset_run", value); 
}
bool ConfigReader::setXVelLimitWalk(double value) { 
    if(std::isnan(value)) {
        cerr << "警告: 试图设置 x_vel_limit_walk 为 NaN，操作被忽略。" << endl;
        return false;
    }
    return writeParameterToFile("x_vel_limit_walk", value); 
}
bool ConfigReader::setXVelLimitRun(double value) { 
    if(std::isnan(value)) {
        cerr << "警告: 试图设置 x_vel_limit_run 为 NaN，操作被忽略。" << endl;
        return false;
    }
    return writeParameterToFile("x_vel_limit_run", value); 
}

// 通用的参数设置方法
bool ConfigReader::setParameter(const std::string& paramName, double value) {
    return writeParameterToFile(paramName, value);
}

// 获取配置文件路径
std::string ConfigReader::getConfigPath() const { return configPath; }

// 设置新的配置文件路径
void ConfigReader::setConfigPath(const std::string& newPath) { 
    configPath = newPath; 
    configLoaded = false; // 路径改变后需要重新加载配置
    parsedParams.clear(); // 清空已解析参数记录
}

// 更新配置文件中的参数值（兼容旧接口）
bool ConfigReader::updateConfigParameter(const std::string& paramName, double value) {
    return writeParameterToFile(paramName, value);
}

// 打印所有参数值
void ConfigReader::printAllParameters() const {
    qDebug() << "\n=== 解析的参数值 (配置文件: " << configPath << ") ===";
    qDebug() << "xsense_data_roll: " << xsense_data_roll;
    qDebug() << "xsense_data_pitch: " << xsense_data_pitch;
    qDebug() << "x_vel_offset: " << x_vel_offset;
    qDebug() << "y_vel_offset: " << y_vel_offset;
    qDebug() << "yaw_vel_offset: " << yaw_vel_offset;
    qDebug() << "x_vel_offset_run: " << x_vel_offset_run;
    qDebug() << "y_vel_offset_run: " << y_vel_offset_run;
    qDebug() << "yaw_vel_offset_run: " << yaw_vel_offset_run;
    qDebug() << "x_vel_limit_walk: " << x_vel_limit_walk;
    qDebug() << "x_vel_limit_run: " << x_vel_limit_run;
    // 打印已解析参数统计
    qDebug() << "已解析参数数量: " << parsedParams.size() << "/" << expectedParams.size();
}

// 检查参数是否存在
bool ConfigReader::isParameterExists(const std::string& paramName) const {
    return parsedParams.find(paramName) != parsedParams.end();
}

// 获取缺失的参数列表
std::vector<std::string> ConfigReader::getMissingParameters() const {
    vector<string> missing;
    for (const auto& param : expectedParams) {
        if (parsedParams.find(param) == parsedParams.end()) {
            missing.push_back(param);
        }
    }
    return missing;
}

bool ConfigReader::writeMultipleParametersToFile(const std::vector<std::pair<std::string, double>>& params) {
    try {
        // 检查参数列表是否为空
        if (params.empty()) {
            qDebug() << "警告: 参数列表为空，无需写入文件";
            return true;
        }
        
        // 检查sshManager是否有效
        if (!sshManager) {
            cerr << "错误: SSH管理器未初始化" << endl;
            return false;
        }
        
        // 检查SSH连接状态
        if (sshManager->isSSHDisconnected()) {
            cerr << "错误: SSH连接已断开，无法写入配置文件" << endl;
            return false;
        }
        
        qDebug() << "开始批量写入" << params.size() << "个参数到配置文件...";
        
        // 计算实际需要写入的参数数量（排除NaN值）
        int validParamCount = 0;
        for (const auto& param : params) {
            const string& paramName = param.first;
            double value = param.second;
            if ((paramName == "x_vel_limit_walk" || paramName == "x_vel_limit_run") && std::isnan(value)) {
                continue;
            }
            validParamCount++;
        }
        
        qDebug() << "开始批量写入参数，原始参数数量:" << params.size() << ", 有效参数数量:" << validParamCount;
        
        qDebug() << "开始批量写入参数，原始参数数量:" << params.size() << ", 有效参数数量:" << validParamCount;
        
        // 读取当前配置文件内容
        string readCommand = "cat " + configPath;
        string fileContent = executeRemoteCommand(readCommand);
        
        if (fileContent.empty()) {
            cerr << "配置文件内容为空或读取失败" << endl;
            return false;
        }
        
        qDebug() << "读取到的配置文件内容:";
        qDebug().noquote() << QString::fromStdString(fileContent);
        
        // 将内容按行分割
        vector<string> lines;
        istringstream contentStream(fileContent);
        string line;
        
        while (getline(contentStream, line)) {
            lines.push_back(line);
        }
        
        qDebug() << "配置文件行数:" << lines.size();
        
        // 批量更新参数值
        for (const auto& param : params) {
            const string& paramName = param.first;
            double value = param.second;
            bool paramFound = false;
            
            // 检查是否为x_vel_limit_walk或x_vel_limit_run且值为NaN，如果是则删除该参数
            if ((paramName == "x_vel_limit_walk" || paramName == "x_vel_limit_run") && std::isnan(value)) {
                qDebug() << "删除参数" << paramName << "，因为值为NaN";
                
                // 从配置文件中删除该参数行
                auto it = lines.begin();
                while (it != lines.end()) {
                    string trimmedLine = *it;
                    trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));
                    
                    if (trimmedLine.find(paramName + "=") == 0) {
                        // 找到参数行，删除它
                        it = lines.erase(it);
                        // 从已解析参数集合中删除
                        parsedParams.erase(paramName);
                        qDebug() << "已从配置文件中删除参数:" << paramName;
                    } else {
                        ++it;
                    }
                }
                continue;
            }
            
            // 在现有行中查找并更新参数
            for (auto& currentLine : lines) {
                string trimmedLine = currentLine;
                trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));
                
                if (trimmedLine.find(paramName + "=") == 0) {
                    // 找到参数行，更新值
                    stringstream newLine;
                    newLine << paramName << "=" << value;
                    currentLine = newLine.str();
                    paramFound = true;
                    break;
                }
            }
            
            // 如果参数不存在，添加到文件末尾
            if (!paramFound) {
                stringstream newLine;
                newLine << paramName << "=" << value;
                lines.push_back(newLine.str());
                parsedParams.insert(paramName); // 添加到已解析集合
            }
            
            // 更新内存中的参数值
            setParameterValue(paramName, value);
        }
        
        // 显示最终要写入的内容
        qDebug() << "最终要写入的配置文件内容:";
        string finalContent;
        for (const auto& currentLine : lines) {
            finalContent += currentLine + "\n";
            qDebug().noquote() << QString::fromStdString(currentLine);
        }
        qDebug() << "最终配置文件行数:" << lines.size();
        
        // 原子写回文件（写临时文件并 mv）
        bool writeOk = atomicWriteRemoteFile(finalContent);
        if (!writeOk) {
            cerr << "批量写入参数失败: 原子写回失败" << endl;
            return false;
        }
        
        qDebug() << "批量写入" << validParamCount << "个参数成功完成! (原始" << params.size() << "个参数)";
        return true;
        
    } catch (const SSHException& e) {
        cerr << "SSH异常: 批量写入参数失败: " << e.what() << endl;
        return false;
    } catch (const exception& e) {
        cerr << "批量写入参数失败: " << e.what() << endl;
        return false;
    } catch (...) {
        cerr << "未知异常: 批量写入参数失败" << endl;
        return false;
    }
}

bool ConfigReader::isConfigLoaded() const { return configLoaded; }