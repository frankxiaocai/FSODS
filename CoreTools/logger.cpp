#include "logger.h"
Logger* Logger::m_instance = nullptr;
QMutex  Logger::m_mutex;

Logger::Logger()
{

}

Logger::~Logger()
{
    // 析构时安全关闭文件
    if (m_logStream.device())
    {
        m_logStream.flush();
        m_logFile.close();
    }
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
    QMutexLocker locker(&m_mutex);
    m_edit = edit;
}

void Logger::writeLog(const QString &level, const QString &msg, QColor color)
{
    QMutexLocker locker(&m_mutex);

    // 格式化日志内容
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logContent = QString("[%1] [%2] %3").arg(time, level, msg);

    // 1. 输出到界面控件
    if (m_edit) {
        m_edit->setTextColor(color);
        m_edit->append(logContent);
        m_edit->moveCursor(QTextCursor::End); // 自动滚动到底部
    }

    // 2. 写入本地文件（程序同级目录）
    QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString logFilePath = QCoreApplication::applicationDirPath() + QString("./out/%1.log").arg(date);
    // 日期变化自动切换日志文件
    if (!m_logFile.isOpen() || m_logFile.fileName() != logFilePath) {
        if (m_logFile.isOpen()) {
            m_logStream.flush();
            m_logFile.close();
        }

        m_logFile.setFileName(logFilePath);
        // 追加模式打开，不存在则创建，文本模式写入
        bool ok = m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        if (ok) {
            m_logStream.setDevice(&m_logFile);
        } else {
            // 文件打开失败，不再写入，避免崩溃
            return;
        }
    }

    // 写入文件并立即刷新
    if (m_logStream.device()) {
        m_logStream << logContent << "\n";
        m_logStream.flush();
    }
}

void Logger::info(const QString& msg)   { writeLog("INFO",  msg, Qt::white); }
void Logger::error(const QString& msg)   { writeLog("ERROR", msg, Qt::red); }