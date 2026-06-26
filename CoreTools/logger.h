#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QTextEdit>
#include <QDateTime>
#include <QLineEdit>
#include <QColor>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QTextCursor>
#include <QCoreApplication>

class Logger
{
public:
    // 单例获取
    static Logger* instance();
    // 设置界面输出控件
    void setTextEdit(QTextEdit* edit);
    // 日志接口
    void info(const QString& msg);//正常流程信息
    void error(const QString& msg);//业务错误

private:
    Logger();
     ~Logger();
    void writeLog(const QString& level, const QString& msg, QColor color);


private:
    static Logger* m_instance;
    QTextEdit* m_edit = nullptr;
    static QMutex  m_mutex;       // 线程安全锁
    QFile          m_logFile;     // 日志文件对象
    QTextStream    m_logStream;   // 文件输出流
};

// 全局宏
#define LOG_INFO(msg)   Logger::instance()->info(msg)
#define LOG_ERROR(msg)  Logger::instance()->error(msg)

#endif // LOGGER_H
