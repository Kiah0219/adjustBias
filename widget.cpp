#include "widget.h"
#include "./ui_widget.h"
#include <QMessageBox>
#include <QRegularExpression>
#include <QProgressDialog>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <iostream>


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
                QMessageBox::information(this, "信息", "保存成功！\n请拍下急停按钮重新启动以使配置生效。");
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
                QMessageBox::warning(this, "加载失败！", "无法连接到指定IP或读取配置文件失败。");
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
            return 1;
        }
    } catch (const SSHException& e) {
        QString msg = QString("SSH连接异常: %1").arg(e.what());
        logException("SSHException", msg, "main_load");
        qDebug() << "SSH连接异常:" << e.what();
        return 1;
    } catch (const std::exception& e) {
        QString msg = QString("加载配置时发生异常: %1").arg(e.what());
        logException("std::exception", msg, "main_load");
        qDebug() << "加载配置时发生异常:" << e.what();
        return 1;
    } catch (...) {
        QString msg = "加载配置时发生未知异常";
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
            
        // 处理特殊的 nan 值
        if(ui->limit_walk_lineEdit->text() != QString("nan")) {
            double limitWalkValue = ui->limit_walk_lineEdit->text().toDouble(&ok);
            if (ok && q_x_vel_limit_walk != limitWalkValue) 
                q_x_vel_limit_walk = limitWalkValue;
        }
        
        if(ui->limit_run_lineEdit->text() != QString("nan")) {
            double limitRunValue = ui->limit_run_lineEdit->text().toDouble(&ok);
            if (ok && q_x_vel_limit_run != limitRunValue) 
                q_x_vel_limit_run = limitRunValue;
        }
    } catch (const std::exception& e) {
        QString msg = QString("解析UI数值时发生异常: %1").arg(e.what());
        logException("std::exception", msg, "main_save");
        qDebug() << "解析UI数值时发生异常:" << e.what();
        return 3; // 数值解析错误
    }

    // 更新 ConfigReader 中的值, 如果有变化则更新写入文件
    if(q_xsense_data_roll != configReader->xsense_data_roll) {
        configReader->xsense_data_roll = q_xsense_data_roll;
        configReader->setXsenseDataRoll(q_xsense_data_roll);
    }
    if(q_xsense_data_pitch != configReader->xsense_data_pitch) {
        configReader->xsense_data_pitch = q_xsense_data_pitch;
        configReader->setXsenseDataPitch(q_xsense_data_pitch);
    }
    if(q_x_vel_offset != configReader->x_vel_offset) {
        configReader->x_vel_offset = q_x_vel_offset;
        configReader->setXVelOffset(q_x_vel_offset);
    }
    if(q_y_vel_offset != configReader->y_vel_offset) {
        configReader->y_vel_offset = q_y_vel_offset;
        configReader->setYVelOffset(q_y_vel_offset);
    }
    if(q_yaw_vel_offset != configReader->yaw_vel_offset) {
        configReader->yaw_vel_offset = q_yaw_vel_offset;
        configReader->setYawVelOffset(q_yaw_vel_offset);    
    }
    if(q_x_vel_offset_run != configReader->x_vel_offset_run) {
        configReader->x_vel_offset_run = q_x_vel_offset_run;
        configReader->setXVelOffsetRun(q_x_vel_offset_run);
    }
    if(q_y_vel_offset_run != configReader->y_vel_offset_run) {
        configReader->y_vel_offset_run = q_y_vel_offset_run;
        configReader->setYVelOffsetRun(q_y_vel_offset_run);
    }
    if(q_yaw_vel_offset_run != configReader->yaw_vel_offset_run) {
        configReader->yaw_vel_offset_run = q_yaw_vel_offset_run;
        configReader->setYawVelOffsetRun(q_yaw_vel_offset_run);
    }
    if(!std::isnan(q_x_vel_limit_walk) && q_x_vel_limit_walk != configReader->x_vel_limit_walk) {
        configReader->x_vel_limit_walk = q_x_vel_limit_walk;
        configReader->setXVelLimitWalk(q_x_vel_limit_walk);
    }
    if(!std::isnan(q_x_vel_limit_run) && q_x_vel_limit_run != configReader->x_vel_limit_run) {
        configReader->x_vel_limit_run = q_x_vel_limit_run;
        configReader->setXVelLimitRun(q_x_vel_limit_run);
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
    ui->limit_walk_lineEdit->setText(QString::number(q_x_vel_limit_walk));
    ui->limit_run_lineEdit->setText(QString::number(q_x_vel_limit_run));

}




void Widget::on_roll_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->roll_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.001
        currentValue += 0.001;
        q_xsense_data_roll = currentValue;
        // 更新回控件
        ui->roll_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_roll_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->roll_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.001
        currentValue -= 0.001;
        q_xsense_data_roll = currentValue;
        // 更新回控件
        ui->roll_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}   
void Widget::on_pitch_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->pitch_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.001
        currentValue += 0.001;
        q_xsense_data_pitch = currentValue;
        // 更新回控件
        ui->pitch_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_pitch_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->pitch_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.001
        currentValue -= 0.001;
        q_xsense_data_pitch = currentValue;
        // 更新回控件
        ui->pitch_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_x_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->x_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.01
        currentValue += 0.01;
        q_x_vel_offset = currentValue;
        // 更新回控件
        ui->x_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_x_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->x_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.01
        currentValue -= 0.01;
        q_x_vel_offset = currentValue;
        // 更新回控件
        ui->x_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}
void Widget::on_y_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->y_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.01
        currentValue += 0.01;
        q_y_vel_offset = currentValue;
        // 更新回控件
        ui->y_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_y_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->y_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.01
        currentValue -= 0.01;
        q_y_vel_offset = currentValue;
        // 更新回控件
        ui->y_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_yaw_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->yaw_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.001
        currentValue += 0.001;
        q_yaw_vel_offset = currentValue;
        // 更新回控件
        ui->yaw_lineEdit->setText(QString::number(currentValue, 'f', 4));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_yaw_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->yaw_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.001
        currentValue -= 0.001;
        q_yaw_vel_offset = currentValue;
        // 更新回控件
        ui->yaw_lineEdit->setText(QString::number(currentValue, 'f', 4));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}
void Widget::on_x_run_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->x_run_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.01
        currentValue += 0.01;
        q_x_vel_offset_run = currentValue;
        // 更新回控件
        ui->x_run_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_x_run_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->x_run_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.01
        currentValue -= 0.01;
        q_x_vel_offset_run = currentValue;
        // 更新回控件
        ui->x_run_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_y_run_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->y_run_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.01
        currentValue += 0.01;
        q_y_vel_offset_run = currentValue;
        // 更新回控件
        ui->y_run_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_y_run_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->y_run_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.01
        currentValue -= 0.01;
        q_y_vel_offset_run = currentValue;
        // 更新回控件
        ui->y_run_lineEdit->setText(QString::number(currentValue, 'f', 3));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}
void Widget::on_yaw_run_plus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->yaw_run_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 增加 0.001
        currentValue += 0.001;
        q_yaw_vel_offset_run = currentValue;
        // 更新回控件
        ui->yaw_run_lineEdit->setText(QString::number(currentValue, 'f', 4));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}

void Widget::on_yaw_run_minus_pushButton_clicked()
{
    try {
        // 获取当前值
        bool ok;
        double currentValue = ui->yaw_run_lineEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "错误", "无效的数值，请检查输入！");
            return;
        }
        // 减少 0.001
        currentValue -= 0.001;
        q_yaw_vel_offset_run = currentValue;
        // 更新回控件
        ui->yaw_run_lineEdit->setText(QString::number(currentValue, 'f', 4));
    } catch (const std::exception& e) {
        QString msg = QString("操作失败: %1").arg(e.what());
        logException("std::exception", msg, "on_roll_plus_pushButton_clicked");
        QMessageBox::warning(this, "错误", msg);
    }
}
