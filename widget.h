#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "SSHManager.hpp"
#include "ConfigReader.hpp"
#include "RemoteCommandExecutor.hpp"
#include "FileHandler.hpp" 
#include "Logger.hpp"


QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    
    // 静态方法用于记录异常日志
    static void logException(const QString& exceptionType, const QString& exceptionMsg, const QString& context = "");

private slots:
    void on_saveButton_clicked();

    void on_loadButton_clicked();
    void on_roll_plus_pushButton_clicked();
    void on_roll_minus_pushButton_clicked();
    void on_pitch_plus_pushButton_clicked();
    void on_pitch_minus_pushButton_clicked();
    void on_x_plus_pushButton_clicked();
    void on_x_minus_pushButton_clicked();
    void on_y_plus_pushButton_clicked();
    void on_y_minus_pushButton_clicked();
    void on_yaw_plus_pushButton_clicked();
    void on_yaw_minus_pushButton_clicked();
    void on_x_run_plus_pushButton_clicked();
    void on_x_run_minus_pushButton_clicked();
    void on_y_run_plus_pushButton_clicked();
    void on_y_run_minus_pushButton_clicked();
    void on_yaw_run_plus_pushButton_clicked();
    void on_yaw_run_minus_pushButton_clicked();



    int main_load();
    int main_save();
    void loadConfigToUI();


private:
    Ui::Widget *ui;
    std::string host = "192.168.1.6";
    const char* username = "ubuntu";
    const char* password = "123";
    const int port = 22;
    const string configPath = "/home/ubuntu/data/param/rl_control_new.txt";
    std::unique_ptr<SSHManager> sshManager;
    std::unique_ptr<ConfigReader> configReader;
    // 最近一次错误信息（用于向用户展示更详细的失败原因）
    std::string lastErrorMessage;


    double q_xsense_data_roll = 0.0;
    double q_xsense_data_pitch = 0.0;
    double q_x_vel_offset = 0.0;
    double q_y_vel_offset = 0.0;
    double q_yaw_vel_offset = 0.0;
    double q_x_vel_offset_run = 0.0;
    double q_y_vel_offset_run = 0.0;
    double q_yaw_vel_offset_run = 0.0;
    double q_x_vel_limit_walk = numeric_limits<double>::quiet_NaN();
    double q_x_vel_limit_run = numeric_limits<double>::quiet_NaN();
        

};
#endif // WIDGET_H