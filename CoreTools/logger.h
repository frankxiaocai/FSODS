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
    static Logger* instance();// 单例
    void setTextEdit(QTextEdit* edit);
    void info(const QString& msg);//正常信息
    void error(const QString& msg);//业务错误

private:
    Logger();
     ~Logger();
    void writeLog(const QString& level, const QString& msg, QColor color);

private:
    static Logger* m_instance;
    QTextEdit* m_edit = nullptr;
};

// 全局宏
#define LOG_INFO(msg)   Logger::instance()->info(msg)
#define LOG_ERROR(msg)  Logger::instance()->error(msg)

#endif // LOGGER_H
