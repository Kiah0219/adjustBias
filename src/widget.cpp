#include "../include/widget.h"
#include "../include/ui_widget.h"
#include <QMessageBox>
#include <QRegularExpression>
#include <QProgressDialog>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <iostream>
#include <cmath>
#include <limits>
#include <limits>


// 添加静态方法用于记录异常日志
void Widget::logException(const QString& exceptionType, const QString& exceptionMsg, const QString& context) {
    try {
        // 确保log文件夹存在
        QDir().mkpath("logs");
        
        // 获取当前日期和时间
        QDateTime now = QDateTime::currentDateTime();
        QString fileName = QString("logs/exception_%1.log")
                        .arg(now.toString("yyyyMMdd_HHmmss_zzz"));
        
        // 写入异常日志
        QFile logFile(fileName);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QTextStream stream(&logFile);
            #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                stream.setEncoding(QStringConverter::Utf8);
            #else
                stream.setCodec("UTF-8");
            #endif
            stream << "Time: " << now.toString("yyyy-MM-dd hh:mm:ss.zzz") << Qt::endl;
            stream << "Exception Type: " << exceptionType << Qt::endl;
            if (!context.isEmpty()) {
                stream << "Context: " << context << Qt::endl;
            }
            stream << "Exception Message: " << exceptionMsg << Qt::endl;
            stream << "----------------------------------------" << Qt::endl;
            logFile.close();
        }
    } catch (...) {
        // 日志记录失败时不抛出异常，避免掩盖原始异常
        std::cout << "无法写入异常日志" << std::endl;
    }
}

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    connect(ui->ip_lineEdit, &QLineEdit::returnPressed, this, &Widget::on_loadButton_clicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &Widget::on_disconnectButton_clicked);    
}

Widget::~Widget()
{
    delete ui;
}



void Widget::on_saveButton_clicked() {
    try{
        // 添加前置检查，避免在资源未初始化时执行
        if (!sshManager || !configReader) {
            QMessageBox::warning(this, "错误", "系统未初始化，请先点击加载按钮！");
            return;
        }
        
        // 检查SSH连接状态
        if (sshManager->isSSHDisconnected()) {
            QMessageBox::warning(this, "错误", "SSH连接已断开，请重新连接！");
            return;
        }
        
        // 额外检查：确保SSH会话仍然有效
        if (!sshManager->getSession()) {
            QMessageBox::warning(this, "错误", "SSH会话无效，请重新连接！");
            return;
        }
        QProgressDialog progressDialog("正在保存配置...", "", 0, 0, this);
        progressDialog.setWindowTitle("保存配置");
        progressDialog.setCancelButton(nullptr); // 移除取消按钮
        progressDialog.setWindowModality(Qt::WindowModal);
        
        // 强制显示对话框
        progressDialog.setMinimumDuration(0); // 立即显示，不延迟
        progressDialog.setValue(0); // 初始化进度

        // 模拟耗时操作（可选）
        QCoreApplication::processEvents();

        // 执行保存逻辑
        int result = main_save();

        // 关闭对话框
        progressDialog.close();


        if (result == 0) {
            QTimer::singleShot(500, [this]() {
                try {
                    // 读取保存后的配置文件内容
                    QString fileContent;
                    if (configReader && sshManager && !sshManager->isSSHDisconnected()) {
                        std::string readCommand = "cat /home/ubuntu/data/param/rl_control_new.txt";
                        std::string content = configReader->executeRemoteCommand(readCommand);
                        fileContent = QString::fromStdString(content);
                        
                        if (!fileContent.isEmpty()) {
                            // 创建自定义对话框显示文件内容
                            QDialog fileDialog(this);
                            fileDialog.setWindowTitle("配置文件内容");
                            fileDialog.setModal(true);
                            fileDialog.resize(600, 400);
                            
                            QVBoxLayout* layout = new QVBoxLayout(&fileDialog);
                            
                            // 添加标题标签
                            QLabel* titleLabel = new QLabel("/home/ubuntu/data/param/rl_control_new.txt 完整内容:", &fileDialog);
                            titleLabel->setStyleSheet("font-weight: bold; color: #2E86AB;");
                            layout->addWidget(titleLabel);
                            
                            // 添加文本编辑框显示文件内容
                            QTextEdit* textEdit = new QTextEdit(&fileDialog);
                            textEdit->setPlainText(fileContent);
                            textEdit->setReadOnly(true);
                            textEdit->setStyleSheet("font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; background-color: #f8f9fa;");
                            layout->addWidget(textEdit);
                            
                            // 添加按钮布局
                            QHBoxLayout* buttonLayout = new QHBoxLayout();
                            
                            QPushButton* okButton = new QPushButton("确定", &fileDialog);
                            okButton->setStyleSheet("background-color: #2E86AB; color: white; padding: 8px 16px; border: none; border-radius: 4px;");
                            okButton->setFixedWidth(80);
                            
                            buttonLayout->addStretch();
                            buttonLayout->addWidget(okButton);
                            layout->addLayout(buttonLayout);
                            
                            // 连接按钮信号
                            connect(okButton, &QPushButton::clicked, &fileDialog, &QDialog::accept);
                            
                            // 保存配置文件到本地桌面"偏置调节记录"文件夹
                            try {
                                // 获取桌面路径
                                QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                                QString recordFolder = desktopPath + "/偏置调节记录";
                                
                                // 确保文件夹存在
                                if (!QDir().mkpath(recordFolder)) {
                                    qDebug() << "无法创建偏置调节记录文件夹";
                                    logException("FileSaveError", "无法创建偏置调节记录文件夹", "on_saveButton_clicked");
                                } else {
                                    // 构造文件名：时间戳-IP.txt
                                    QDateTime now = QDateTime::currentDateTime();
                                    QString timestamp = now.toString("yyyyMMdd_HHmmss");
                                    QString ipAddr = QString::fromStdString(host);
                                    QString fileName = QString("%1-%2.txt").arg(timestamp, ipAddr);
                                    QString filePath = recordFolder + "/" + fileName;
                                    
                                    // 写入文件
                                    QFile recordFile(filePath);
                                    if (recordFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                        QTextStream out(&recordFile);
                                        #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                                            out.setEncoding(QStringConverter::Utf8);
                                        #else
                                            out.setCodec("UTF-8");
                                        #endif
                                        
                                        // 写入头信息
                                        out << "========================================" << "\n";
                                        out << "偏置调节记录" << "\n";
                                        out << "========================================" << "\n";
                                        out << "保存时间: " << now.toString("yyyy-MM-dd hh:mm:ss") << "\n";
                                        out << "IP地址: " << ipAddr << "\n";
                                        out << "远程文件路径: /home/ubuntu/data/param/rl_control_new.txt" << "\n";
                                        out << "========================================" << "\n\n";
                                        
                                        // 写入配置内容
                                        out << fileContent;
                                        
                                        recordFile.close();
                                        qDebug() << "配置文件已保存到: " << filePath;
                                    } else {
                                        qDebug() << "无法打开文件进行写入: " << filePath;
                                        logException("FileSaveError", 
                                                   QString("无法打开文件: %1").arg(filePath),
                                                   "on_saveButton_clicked");
                                    }
                                }
                            } catch (const std::exception& e) {
                                qDebug() << "保存配置文件到本地时发生异常:" << e.what();
                                logException("std::exception",
                                           QString("保存配置文件到本地时发生异常: %1").arg(e.what()),
                                           "on_saveButton_clicked");
                            }
                            
                            // 显示对话框
                            fileDialog.exec();
                            
                            // 显示保存成功提示
                            QMessageBox::information(this, "信息", "保存成功！\n请拍下急停按钮重新启动以使配置生效。\n\n已将配置内容保存在本地桌面「偏置调节记录」文件夹。");
                        } else {
                            QMessageBox::information(this, "信息", "保存成功！\n请拍下急停按钮重新启动以使配置生效。\n\n注意：无法读取配置文件内容显示。");
                        }
                    } else {
                        QMessageBox::information(this, "信息", "保存成功！\n请拍下急停按钮重新启动以使配置生效。\n\n注意：连接状态异常，无法读取配置文件内容。");
                    }
                } catch (const std::exception& e) {
                    QMessageBox::information(this, "信息", "保存成功！\n请拍下急停按钮重新启动以使配置生效。\n\n注意：读取配置文件时发生异常：" + QString(e.what()));
                }
            });
        } else if (result == 1) {
            QTimer::singleShot(500, [this]() {
                QMessageBox::warning(this, "信息", "保存失败！\n请先加载。");
            });
        } else if (result == 2) {
            QTimer::singleShot(500, [this]() {
                QMessageBox::warning(this, "信息", "保存失败！\nSSH连接已断开，请重新加载。");
            });
        }
 
        return;
    }catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("保存过程中出现异常:\n %1").arg(e.what()));
    }
}




void Widget::on_loadButton_clicked()
{
    try{


        QString ipText = ui->ip_lineEdit->text();
        QRegularExpression ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
        if (!ipRegex.match(ipText).hasMatch()) {
            QMessageBox::warning(this, "错误", "请输入有效的IP地址！");
            return;
        }
        // 更新 host 变量
        // host = ipText.toStdString();
        host = ipText.toStdString();
        // 检查现有的 SSH 连接是否有效且匹配当前 IP 地址
        // 使用新的断连检测函数替代原来的 session 标志判断
        if (sshManager && configReader && !sshManager->isSSHDisconnected() && sshManager->getHost() == string(host)) {
            QMessageBox::information(this, "SSH有效，无需重连！", QString("当前IP: %1\n参数信息已重新加载！").arg(sshManager->getHost()));
            loadConfigToUI();
            return;
        }

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "确认", QString("当前IP为: %1，是否继续？").arg(host),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            // // 用户点击了“是”
            QMessageBox loadingBox;
            loadingBox.setWindowTitle("加载中");
            loadingBox.setText("正在加载配置，请稍候...");
            loadingBox.setStandardButtons(QMessageBox::NoButton);
            loadingBox.setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
            loadingBox.setModal(true);
            loadingBox.show();
            QApplication::processEvents();
            loadingBox.activateWindow();
            loadingBox.raise();
            QApplication::processEvents();
            int result = main_load();
            loadingBox.close();
            if(result == 0) {
                QMessageBox::information(this, "信息已加载！", QString("连接到IP: %1").arg(host));
            }
            else if (result == 1) {
                // 尽可能显示更详细的错误信息，方便排查
                QString detail = QString::fromStdString(lastErrorMessage.empty() ? "无法连接到指定IP或读取配置文件失败。" : lastErrorMessage);
                QMessageBox::warning(this, "加载失败！", QString("无法连接到指定IP或读取配置文件失败。\n%1").arg(detail));
            }else {
                QMessageBox::warning(this, "加载失败！", "SSH连接已断开！请重新加载。");
            }

            return;
        }
        else {  
            // 用户点击了“否”
            return;
        }
    }catch (const std::exception& e) {
        // 警告框
        QMessageBox::warning(this, "错误", QString("加载过程中出现异常:\n %1").arg(e.what()));
    }
    
}


int Widget::main_load() {
    qDebug() << "SSH参数 - Host:" << QString::fromStdString(host) 
         << "Port:" << port << "User:" << QString::fromStdString(username);

    try {
        sshManager = std::make_unique<SSHManager>(host, username, password, port);
        configReader = std::make_unique<ConfigReader>(sshManager.get(), configPath);        
        if (configReader->loadConfig()) {
            // 设置编辑框的值
            loadConfigToUI();
            return 0;
        } else {
            // 在无法读取配置但未抛出异常的情况下，设置通用错误信息
            lastErrorMessage = "无法读取远程配置文件或配置文件校验失败";
            return 1;
        }
    } catch (const ApplicationException& e) {
        // 记录并保存详细错误信息，返回失败
        lastErrorMessage = std::string("应用异常: ") + e.what();
        QString msg = QString::fromStdString(lastErrorMessage);
        logException("ApplicationException", msg, "main_load");
        qDebug() << "应用异常:" << msg;
        return 1;
    } catch (const std::exception& e) {
        lastErrorMessage = std::string("加载配置时发生异常: ") + e.what();
        QString msg = QString::fromStdString(lastErrorMessage);
        logException("std::exception", msg, "main_load");
        qDebug() << "加载配置时发生异常:" << msg;
        return 1;
    } catch (...) {
        lastErrorMessage = "加载配置时发生未知异常";
        QString msg = QString::fromStdString(lastErrorMessage);
        logException("Unknown Exception", msg, "main_load");
        qDebug() << "加载配置时发生未知异常";
        return 1;
    }
}



int Widget::main_save(){
    // 检查资源是否初始化
    if (sshManager == nullptr || configReader == nullptr) {
        qDebug() << "错误: SSHManager 或 ConfigReader 未初始化";
        return 1; // SSHManager 或 ConfigReader 未初始化
    }
    
    // 检查SSH连接状态
    if(sshManager->isSSHDisconnected()) {
        qDebug() << "错误: SSH连接已断开";
        return 2; // SSH 连接无效
    }
    
    // 额外检查：确保SSH会话仍然有效
    if (!sshManager->getSession()) {
        qDebug() << "错误: 无法获取有效的SSH会话";
        return 2; // SSH 连接无效
    }

    try {
        // 获取当前 UI 上的值并更新到成员变量，添加异常处理
        bool ok;
        double rollValue = ui->roll_lineEdit->text().toDouble(&ok);
        if (ok && q_xsense_data_roll != rollValue) 
            q_xsense_data_roll = rollValue;
            
        double pitchValue = ui->pitch_lineEdit->text().toDouble(&ok);
        if (ok && q_xsense_data_pitch != pitchValue) 
            q_xsense_data_pitch = pitchValue;
            
        double xValue = ui->x_lineEdit->text().toDouble(&ok);
        if (ok && q_x_vel_offset != xValue) 
            q_x_vel_offset = xValue;
            
        double yValue = ui->y_lineEdit->text().toDouble(&ok);
        if (ok && q_y_vel_offset != yValue) 
            q_y_vel_offset = yValue;
            
        double yawValue = ui->yaw_lineEdit->text().toDouble(&ok);
        if (ok && q_yaw_vel_offset != yawValue) 
            q_yaw_vel_offset = yawValue;
            
        double xRunValue = ui->x_run_lineEdit->text().toDouble(&ok);
        if (ok && q_x_vel_offset_run != xRunValue) 
            q_x_vel_offset_run = xRunValue;
            
        double yRunValue = ui->y_run_lineEdit->text().toDouble(&ok);
        if (ok && q_y_vel_offset_run != yRunValue) 
            q_y_vel_offset_run = yRunValue;
            
        double yawRunValue = ui->yaw_run_lineEdit->text().toDouble(&ok);
        if (ok && q_yaw_vel_offset_run != yawRunValue) 
            q_yaw_vel_offset_run = yawRunValue;
            
        // 处理限速值：如果输入不是数字，则不写入
        // ===== Optimization #6: 识别"未设置"文本并按NaN处理 =====
        if(ui->limit_walk_lineEdit->text() == QString("nan") || 
           ui->limit_walk_lineEdit->text() == QString("未设置")) {
            // 如果UI中限速值为nan或"未设置"，设置成员变量为NaN，触发删除逻辑
            q_x_vel_limit_walk = std::numeric_limits<double>::quiet_NaN();
            qDebug() << "检测到x_vel_limit_walk为nan或未设置，将触发删除逻辑";
        } else {
            double limitWalkValue = ui->limit_walk_lineEdit->text().toDouble(&ok);
            if (ok) {
                if (q_x_vel_limit_walk != limitWalkValue) 
                    q_x_vel_limit_walk = limitWalkValue;
            } else {
                // 如果输入不是有效数字，显示警告但不更新成员变量
                qDebug() << "警告: x_vel_limit_walk输入不是有效数字，将跳过写入";
                QMessageBox::warning(nullptr, "输入错误", "行走速度限制值不是有效数字，将跳过保存！");
            }
        }
        
        if(ui->limit_run_lineEdit->text() == QString("nan") || 
           ui->limit_run_lineEdit->text() == QString("未设置")) {
            // 如果UI中限速值为nan或"未设置"，设置成员变量为NaN，触发删除逻辑
            q_x_vel_limit_run = std::numeric_limits<double>::quiet_NaN();
            qDebug() << "检测到x_vel_limit_run为nan或未设置，将触发删除逻辑";
        } else {
            double limitRunValue = ui->limit_run_lineEdit->text().toDouble(&ok);
            if (ok) {
                if (q_x_vel_limit_run != limitRunValue) 
                    q_x_vel_limit_run = limitRunValue;
            } else {
                // 如果输入不是有效数字，显示警告但不更新成员变量
                qDebug() << "警告: x_vel_limit_run输入不是有效数字，将跳过写入";
                QMessageBox::warning(nullptr, "输入错误", "跑步速度限制值不是有效数字，将跳过保存！");
            }
        }
    } catch (const std::exception& e) {
        QString msg = QString("解析UI数值时发生异常: %1").arg(e.what());
        logException("std::exception", msg, "main_save");
        qDebug() << "解析UI数值时发生异常:" << e.what();
        return 3; // 数值解析错误
    }

    // 使用批量更新方法一次性写入所有参数到配置文件
    bool success = configReader->updateMultipleParameters(
        q_xsense_data_roll,
        q_xsense_data_pitch,
        q_x_vel_offset,
        q_y_vel_offset,
        q_yaw_vel_offset,
        q_x_vel_offset_run,
        q_y_vel_offset_run,
        q_yaw_vel_offset_run,
        q_x_vel_limit_walk,
        q_x_vel_limit_run
    );

    if (!success) {
        QString msg = "批量更新参数到配置文件失败";
        logException("ConfigError", msg, "main_save");
        qDebug() << msg;
        return 4; // 配置文件写入失败
    }

    return 0;

    
}



void Widget::loadConfigToUI() {

    
    q_xsense_data_roll = configReader->getXsenseDataRoll();
    q_xsense_data_pitch = configReader->getXsenseDataPitch();
    q_x_vel_offset = configReader->getXVelOffset();
    q_y_vel_offset = configReader->getYVelOffset();
    q_yaw_vel_offset = configReader->getYawVelOffset();
    q_x_vel_offset_run = configReader->getXVelOffsetRun(); 
    q_y_vel_offset_run = configReader->getYVelOffsetRun();
    q_yaw_vel_offset_run = configReader->getYawVelOffsetRun();
    q_x_vel_limit_walk = configReader->getXVelLimitWalk();
    q_x_vel_limit_run = configReader->getXVelLimitRun();

    ui->roll_lineEdit->setText(QString::number(q_xsense_data_roll));
    ui->pitch_lineEdit->setText(QString::number(q_xsense_data_pitch));
    ui->x_lineEdit->setText(QString::number(q_x_vel_offset));
    ui->y_lineEdit->setText(QString::number(q_y_vel_offset));
    ui->yaw_lineEdit->setText(QString::number(q_yaw_vel_offset, 'f', 4));
    ui->x_run_lineEdit->setText(QString::number(q_x_vel_offset_run));
    ui->y_run_lineEdit->setText(QString::number(q_y_vel_offset_run));
    ui->yaw_run_lineEdit->setText(QString::number(q_yaw_vel_offset_run, 'f', 4));
    
    // ===== Optimization #6: 为未设置的limit值显示中文提示 =====
    // 加载时，如果limit值为NaN，显示"未设置"而不是"nan"
    if (std::isnan(q_x_vel_limit_walk)) {
        ui->limit_walk_lineEdit->setText("未设置");
    } else {
        ui->limit_walk_lineEdit->setText(QString::number(q_x_vel_limit_walk));
    }
    
    if (std::isnan(q_x_vel_limit_run)) {
        ui->limit_run_lineEdit->setText("未设置");
    } else {
        ui->limit_run_lineEdit->setText(QString::number(q_x_vel_limit_run));
    }

}




// 通用的按钮点击处理函数模板
void Widget::adjustParameter(QLineEdit* lineEdit, double& memberVar, double delta, int precision) {
    try {
        // 获取当前值
        bool ok;
        double currentValue = lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 调整数值
        currentValue += delta;
        memberVar = currentValue;
        // 更新回控件
        lineEdit->setText(QString::number(currentValue, 'f', precision));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "adjustParameter");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_roll_plus_pushButton_clicked() {
    adjustParameter(ui->roll_lineEdit, q_xsense_data_roll, 0.001, 3);
}

void Widget::on_roll_minus_pushButton_clicked() {
    adjustParameter(ui->roll_lineEdit, q_xsense_data_roll, -0.001, 3);
}   

void Widget::on_pitch_plus_pushButton_clicked() {
    adjustParameter(ui->pitch_lineEdit, q_xsense_data_pitch, 0.001, 3);
}

void Widget::on_pitch_minus_pushButton_clicked() {
    adjustParameter(ui->pitch_lineEdit, q_xsense_data_pitch, -0.001, 3);
}

void Widget::on_x_plus_pushButton_clicked() {
    adjustParameter(ui->x_lineEdit, q_x_vel_offset, 0.01, 3);
}

void Widget::on_x_minus_pushButton_clicked() {
    adjustParameter(ui->x_lineEdit, q_x_vel_offset, -0.01, 3);
}

void Widget::on_y_plus_pushButton_clicked() {
    adjustParameter(ui->y_lineEdit, q_y_vel_offset, 0.01, 3);
}

void Widget::on_y_minus_pushButton_clicked() {
    adjustParameter(ui->y_lineEdit, q_y_vel_offset, -0.01, 3);
}

void Widget::on_yaw_plus_pushButton_clicked() {
    adjustParameter(ui->yaw_lineEdit, q_yaw_vel_offset, 0.001, 4);
}

void Widget::on_yaw_minus_pushButton_clicked() {
    adjustParameter(ui->yaw_lineEdit, q_yaw_vel_offset, -0.001, 4);
}

void Widget::on_x_run_plus_pushButton_clicked() {
    adjustParameter(ui->x_run_lineEdit, q_x_vel_offset_run, 0.01, 3);
}

void Widget::on_x_run_minus_pushButton_clicked() {
    adjustParameter(ui->x_run_lineEdit, q_x_vel_offset_run, -0.01, 3);
}

void Widget::on_y_run_plus_pushButton_clicked() {
    adjustParameter(ui->y_run_lineEdit, q_y_vel_offset_run, 0.01, 3);
}

void Widget::on_y_run_minus_pushButton_clicked() {
    adjustParameter(ui->y_run_lineEdit, q_y_vel_offset_run, -0.01, 3);
}

void Widget::on_yaw_run_plus_pushButton_clicked() {
    adjustParameter(ui->yaw_run_lineEdit, q_yaw_vel_offset_run, 0.001, 4);
}

void Widget::on_yaw_run_minus_pushButton_clicked() {
    adjustParameter(ui->yaw_run_lineEdit, q_yaw_vel_offset_run, -0.001, 4);
}

void Widget::on_disconnectButton_clicked() {
    try {
        // 快速检查是否有SSH连接需要断开
        if (!sshManager) {
            QMessageBox::information(this, "信息", "SSH连接未建立连接！");
            return;
        }
        
        // 创建进度对话框，与加载和保存按钮保持一致
        QProgressDialog progressDialog("正在断开SSH连接...", "", 0, 0, this);
        progressDialog.setWindowTitle("断开连接");
        progressDialog.setCancelButton(nullptr); // 移除取消按钮
        progressDialog.setWindowModality(Qt::WindowModal);
        
        // 强制显示对话框
        progressDialog.setMinimumDuration(0); // 立即显示，不延迟
        progressDialog.setValue(0); // 初始化进度
        
        // 模拟耗时操作（可选）
        QCoreApplication::processEvents();
        
        // 执行断开连接逻辑
        sshManager->invalidateSession();
        
        // 快速清除UI界面上所有lineEdit的内容
        ui->roll_lineEdit->clear();
        ui->pitch_lineEdit->clear();
        ui->x_lineEdit->clear();
        ui->y_lineEdit->clear();
        ui->yaw_lineEdit->clear();
        ui->x_run_lineEdit->clear();
        ui->y_run_lineEdit->clear();
        ui->yaw_run_lineEdit->clear();
        ui->limit_walk_lineEdit->clear();
        ui->limit_run_lineEdit->clear();
        
        // 关闭对话框
        progressDialog.close();
        
        // 显示断开成功消息
        QMessageBox::information(this, "信息", "SSH连接已成功断开！\n界面内容已清空。");
        
        qDebug() << "SSH连接已主动断开，UI界面已清空";
        
    } catch (const std::exception& e) {
        QString msg = QString("断开SSH连接时发生异常: %1").arg(e.what());
        logException("std::exception", msg, "on_disconnectButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}
