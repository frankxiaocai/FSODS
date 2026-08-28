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

    // 追加写入数据
   int saveDataGroupToCsv(const QString &timeStr, int channel, int quality, const QVector<float> &waveLength, const QVector<float> &originalSpectrum, const QVector<float> &analyzeSpectrum);
private:
    FileIO();
    ~FileIO();
    static FileIO* m_instance;

    // 配置文件路径
    const QString m_configPath = "config.ini";
    // 光谱存储文件路径
    QString m_csvFilePath = QString("E/larmanOut/%1_spectualdata.csv").arg(QDate::currentDate().toString("yyyyMMdd"));
};

#endif // FILEIO_H
