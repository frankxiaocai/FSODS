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

    // 写入所有配置到文件
    bool writeConfig(const QVector<scanConfigs>& configVec, const otherConfigs& otherCfg);

    // 从文件读取所有配置
    bool readConfig(QVector<scanConfigs>& configVec);
    bool readConfig(otherConfigs& otherCfg);

    // 追加写入数据
    int saveDataGroupToCsv(const QString &timeStr, int channel, int quality, const QVector<float> &m_waveLength, const QVector<float> &m_originalSpectrum, const QVector<float> &m_analyzeSpectrum);

private:
    FileIO();
    ~FileIO();
    static FileIO* m_instance;
    // 自动回收单例（程序退出时自动 delete）
    struct AutoDelete {
        ~AutoDelete() {
            if (m_instance) {
                delete m_instance;
                m_instance = nullptr;
            }
        }
    };
    static AutoDelete m_autoDelete;

    // 配置文件路径
    const QString m_configPath = "config.ini";
    // 光谱存储文件路径
    QString m_csvFilePath = QString("./out/%1_spectualdata.csv").arg(QDate::currentDate().toString("yyyyMMdd"));
};

#endif // FILEIO_H
