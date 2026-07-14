#ifndef FILEIO_H
#define FILEIO_H

#include "mystruct.h"
#include <QVector>
#include <QDebug>
#include <QObject>
#include <QSettings>
#include <QFile>
#include <QDate>

class FileIO
{
public:
    // 单例获取
    static FileIO* instance();

    bool writeDianKongConfig(const diankongConfigs& cfg);
    bool readDianKongConfig(diankongConfigs& cfg);

private:
    FileIO();
    ~FileIO();
    static FileIO* m_instance;

    // 配置文件路径
    const QString m_configPath = "config.ini";
};

#endif // FILEIO_H
