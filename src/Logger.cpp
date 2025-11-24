#include "Logger.h"

Logger::DualStreamBuf::DualStreamBuf(streambuf* screenBuf, streambuf* fileBuf)
    : screenBuf(screenBuf), fileBuf(fileBuf) {}
    
int Logger::DualStreamBuf::overflow(int c) {
    if (c == EOF) return !EOF;
    screenBuf->sputc(c);
    fileBuf->sputc(c);
    return c;
}
    
int Logger::DualStreamBuf::sync() {
    screenBuf->pubsync();
    fileBuf->pubsync();
    return 0;
}

Logger::Logger(const string& filename) {
    // 打开日志文件
    logFile.open(filename);
    if (!logFile.is_open()) {
        throw runtime_error("无法创建日志文件: " + filename);
    }
    
    // 保存原始缓冲区
    coutOriginalBuf = cout.rdbuf();
    cerrOriginalBuf = cerr.rdbuf();
    
    // 创建双重输出缓冲区
    dualBuf = make_unique<DualStreamBuf>(coutOriginalBuf, logFile.rdbuf());
    
    // 重定向标准输出
    cout.rdbuf(dualBuf.get());
    cerr.rdbuf(dualBuf.get());
}

Logger::~Logger() {
    // 恢复原始输出设置
    cout.rdbuf(coutOriginalBuf);
    cerr.rdbuf(cerrOriginalBuf);
    
    // 关闭日志文件
    if (logFile.is_open()) {
        logFile.close();
    }
}

// 显式关闭日志（通常在程序结束前调用）
void Logger::close() {
    if (logFile.is_open()) {
        logFile.close();
    }
}