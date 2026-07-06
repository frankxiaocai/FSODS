#include "logger.h"
Logger* Logger::m_instance = nullptr;

Logger::Logger()
{

}

Logger::~Logger()
{
    qDebug() << "Logger 单例析构释放";
}

Logger *Logger::instance()
{
    if (m_instance == nullptr)
    {
        m_instance = new Logger;
    }
    return m_instance;
}

void Logger::setTextEdit(QTextEdit *edit)
{
    m_edit = edit;
}

void Logger::writeLog(const QString &level, const QString &msg, QColor color)
{
    // 格式化日志内容
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logContent = QString("[%1] [%2] %3").arg(time, level, msg);

    // 1. 输出日志内容到界面控件
    if (m_edit) {
        m_edit->setTextColor(color);
        m_edit->append(logContent);
        m_edit->moveCursor(QTextCursor::End);
    }
}

void Logger::info(const QString& msg)   { writeLog("INFO",  msg, Qt::white); }
void Logger::error(const QString& msg)   { writeLog("ERROR", msg, Qt::red); }