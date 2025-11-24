#include "ConfigReader.h"
#include <QDebug>

bool ConfigReader::validateConfigFile(const string& filePath) {
    ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    // 简单验证文件内容是否为空
    string line;
    getline(file, line);
    return !line.empty();
}

void ConfigReader::setParameterValue(const string& varName, double value) {
    if (varName == "xsense_data_roll") {
        xsense_data_roll = value;
    } else if (varName == "xsense_data_pitch") {
        xsense_data_pitch = value;
    } else if (varName == "x_vel_offset") {
        x_vel_offset = value;
    } else if (varName == "y_vel_offset") {
        y_vel_offset = value;
    } else if (varName == "yaw_vel_offset") {
        yaw_vel_offset = value;
    } else if (varName == "x_vel_offset_run") {
        x_vel_offset_run = value;
    } else if (varName == "y_vel_offset_run") {
        y_vel_offset_run = value;
    } else if (varName == "yaw_vel_offset_run") {
        yaw_vel_offset_run = value;
    } else if (varName == "x_vel_limit_walk") {
        x_vel_limit_walk = value;
    } else if (varName == "x_vel_limit_run") {
        x_vel_limit_run = value;
    }
}

ConfigReader::ConfigReader(SSHManager* manager, const string& configPath) 
    : sshManager(manager), configPath(configPath) {}

string ConfigReader::executeRemoteCommand(const string& command, int maxRetries) {
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
            string result;

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
                throw SSHException(string("执行命令时发生异常: ") + e.what());
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
            cerr << "错误: SSH管理器未初始化，无法创建默认配置文件" << endl;
            return false;
        }
        
        qDebug() << "正在创建默认配置文件: " << configPath;
        
        // 创建目录（如果不存在）
        size_t lastSlash = configPath.find_last_of('/');
        if (lastSlash != string::npos) {
            string dirPath = configPath.substr(0, lastSlash);
            string mkdirCommand = "mkdir -p " + dirPath;
            string result = executeRemoteCommand(mkdirCommand);
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
        
        // 创建配置文件
        string createCommand = "cat > " + configPath + " << 'EOF'\n" + defaultConfig + "EOF";
        string result = executeRemoteCommand(createCommand);
        
        // 检查文件是否创建成功
        string checkCommand = "test -f " + configPath + " && echo \"created\" || echo \"failed\"";
        result = executeRemoteCommand(checkCommand);
        
        if (result.find("created") != string::npos) {
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

void ConfigReader::parseConfigContent(const string& content) {
    parsedParams.clear(); // 清空已解析参数集合
    istringstream contentStream(content);
    string line;
    
    while (getline(contentStream, line)) {
        // 去除行首尾空白字符
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // 跳过空行和注释行
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 查找等号分隔符
        size_t equalPos = line.find('=');
        if (equalPos != string::npos) {
            string varName = line.substr(0, equalPos);
            string valueStr = line.substr(equalPos + 1);
            
            // 去除变量名和值的空白字符
            varName.erase(0, varName.find_first_not_of(" \t"));
            varName.erase(varName.find_last_not_of(" \t") + 1);
            valueStr.erase(0, valueStr.find_first_not_of(" \t"));
            valueStr.erase(valueStr.find_last_not_of(" \t") + 1);
            
            try {
                // 验证数值字符串格式
                if (valueStr.empty() || valueStr.find_first_not_of("0123456789.-+eE") != string::npos) {
                    cerr << "无效的数值格式: " << line << endl;
                    continue;
                }
                
                double value = stod(valueStr);
                // 检查是否为有限数值
                if (!std::isfinite(value)) {
                    cerr << "数值不是有限数: " << line << endl;
                    continue;
                }
                
                setParameterValue(varName, value);
                parsedParams.insert(varName); // 记录已解析的参数
            } catch (const std::invalid_argument& e) {
                cerr << "数值格式错误: " << line << " - " << e.what() << endl;
            } catch (const std::out_of_range& e) {
                cerr << "数值超出范围: " << line << " - " << e.what() << endl;
            } catch (const exception& e) {
                cerr << "解析数值失败: " << line << " - " << e.what() << endl;
            }
        }
    }
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
        
        // 写回文件
        string writeCommand = "cat > " + configPath + " << 'EOF'\n" + 
                             newContent.str() + "EOF";
        
        string result = executeRemoteCommand(writeCommand);
        
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

bool ConfigReader::writeParameterToFile(const string& paramName, double value) {
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

        
        // 重新写入文件
        string writeCommand = "cat > " + configPath + " << 'EOF'\n";
        for (const auto& currentLine : lines) {
            writeCommand += currentLine + "\n";
        }
        writeCommand += "EOF";
        
        string result = executeRemoteCommand(writeCommand);
        
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
            
            // 解析文件内容
            parseConfigContent(fileContent);
            
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
                
                // 解析文件内容
                parseConfigContent(fileContent);
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
bool ConfigReader::setParameter(const string& paramName, double value) {
    return writeParameterToFile(paramName, value);
}

// 获取配置文件路径
string ConfigReader::getConfigPath() const { return configPath; }

// 设置新的配置文件路径
void ConfigReader::setConfigPath(const string& newPath) { 
    configPath = newPath; 
    configLoaded = false; // 路径改变后需要重新加载配置
    parsedParams.clear(); // 清空已解析参数记录
}

// 更新配置文件中的参数值（兼容旧接口）
bool ConfigReader::updateConfigParameter(const string& paramName, double value) {
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
bool ConfigReader::isParameterExists(const string& paramName) const {
    return parsedParams.find(paramName) != parsedParams.end();
}

// 获取缺失的参数列表
vector<string> ConfigReader::getMissingParameters() const {
    vector<string> missing;
    for (const auto& param : expectedParams) {
        if (parsedParams.find(param) == parsedParams.end()) {
            missing.push_back(param);
        }
    }
    return missing;
}

bool ConfigReader::writeMultipleParametersToFile(const vector<pair<string, double>>& params) {
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
        
        while (getline(contentStream, line)) {
            lines.push_back(line);
        }
        
        // 批量更新参数值
        for (const auto& param : params) {
            const string& paramName = param.first;
            double value = param.second;
            bool paramFound = false;
            
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
        
        // 重新写入文件
        string writeCommand = "cat > " + configPath + " << 'EOF'\n";
        for (const auto& currentLine : lines) {
            writeCommand += currentLine + "\n";
        }
        writeCommand += "EOF";
        
        string result = executeRemoteCommand(writeCommand);
        
        qDebug() << "批量写入" << params.size() << "个参数成功完成!";
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