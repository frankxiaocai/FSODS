#include "fileio.h"
FileIO* FileIO::m_instance = nullptr;

FileIO::FileIO() {}
FileIO::~FileIO()
{
   qDebug() << "FileIO 单例析构释放";
}

FileIO *FileIO::instance()
{
    if (m_instance == nullptr)
    {
        m_instance = new FileIO;
    }
    return m_instance;
}

bool FileIO::writeDianKongConfig(const diankongConfigs &cfg)
{
    QSettings setting(m_configPath, QSettings::IniFormat);
    // 分组 [DianKong]
    setting.beginGroup("DianKong");

    setting.setValue("delayMsL0", cfg.delayMsL0);
    setting.setValue("delayMsL1", cfg.delayMsL1);
    setting.setValue("delayMsL2", cfg.delayMsL2);
    setting.setValue("delayMsL3", cfg.delayMsL3);
    setting.setValue("delayMsL4", cfg.delayMsL4);
    setting.setValue("delayMsLarman", cfg.delayMsLarman);

    setting.setValue("Exposure", cfg.Exposure);
    setting.setValue("FrameRate", cfg.FrameRate);
    setting.setValue("XLines", cfg.XLines);

    setting.endGroup();

    setting.sync();
    return setting.status() == QSettings::NoError;
}

bool FileIO::readDianKongConfig(diankongConfigs& cfg)
{
    QSettings setting(m_configPath, QSettings::IniFormat);
    if (!QFile::exists(m_configPath))
        return false;

    setting.beginGroup("DianKong");

    // 存在key就读取，不存在自动使用结构体初始默认值
    if (setting.contains("delayMsL0"))
        cfg.delayMsL0 = setting.value("delayMsL0").toInt();
    if (setting.contains("delayMsL1"))
        cfg.delayMsL1 = setting.value("delayMsL1").toInt();
    if (setting.contains("delayMsL2"))
        cfg.delayMsL2 = setting.value("delayMsL2").toInt();
    if (setting.contains("delayMsL3"))
        cfg.delayMsL3 = setting.value("delayMsL3").toInt();
    if (setting.contains("delayMsL4"))
        cfg.delayMsL4 = setting.value("delayMsL4").toInt();
    if (setting.contains("delayMsLarman"))
        cfg.delayMsLarman = setting.value("delayMsLarman").toInt();

    if (setting.contains("Exposure"))
        cfg.Exposure = setting.value("Exposure").toDouble();
    if (setting.contains("FrameRate"))
        cfg.FrameRate = setting.value("FrameRate").toDouble();
    if (setting.contains("XLines"))
        cfg.XLines = setting.value("XLines").toInt();

    setting.endGroup();

    return setting.status() == QSettings::NoError;
}