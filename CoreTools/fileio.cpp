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
    setting.setValue("delayMs_afterW1", cfg.delayMs_afterW1);
    setting.setValue("delayMs_afterW2", cfg.delayMs_afterW2);

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
    if (setting.contains("delayMs_afterW1"))
        cfg.delayMs_afterW1 = setting.value("delayMs_afterW1").toInt();
    if (setting.contains("delayMs_afterW2"))
        cfg.delayMs_afterW2 = setting.value("delayMs_afterW2").toInt();

    if (setting.contains("Exposure"))
        cfg.Exposure = setting.value("Exposure").toDouble();
    if (setting.contains("FrameRate"))
        cfg.FrameRate = setting.value("FrameRate").toDouble();
    if (setting.contains("XLines"))
        cfg.XLines = setting.value("XLines").toInt();

    setting.endGroup();

    return setting.status() == QSettings::NoError;
}

int FileIO::saveDataGroupToCsv(const QString &timeStr, int channel, int quality, const QVector<float> &waveLength, const QVector<float> &originalSpectrum, const QVector<float> &analyzeSpectrum)
{
    QFile file(m_csvFilePath);
    bool isNewFile = !file.exists();
    int rowCount = waveLength.size();

    // 1. 新文件：创建第一组数据
    if (isNewFile) {
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "无法创建文件：" << m_csvFilePath;
            return 0;
        }
        QTextStream out(&file);
        //out.setCodec("UTF-8"); // 避免中文表头乱码
        // 写表头
        out << "时间,通道,质量,波长,原始光谱,解析光谱\n";
        // 写数据行
        for (int row = 0; row < rowCount; ++row) {
            if (row == 0) {
                // 首行：时间+通道
                out << QString("%1,%2,%3,%4,%5,%6\n")
                           .arg(timeStr)
                           .arg(channel)
                           .arg(quality)
                           .arg(waveLength[row])
                           .arg(originalSpectrum[row])
                           .arg(analyzeSpectrum[row]);
            } else {
                // 非首行：时间+通道留空
                out << QString(",,,%1,%2,%3\n")
                           .arg(waveLength[row])
                           .arg(originalSpectrum[row])
                           .arg(analyzeSpectrum[row]);
            }
        }
        file.close();
        return 1;
    }

    // 2. 已有文件：读取所有行，横向追加新列（核心修正点）
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qDebug() << "无法打开文件：" << m_csvFilePath;
        return 0;
    }

    QTextStream in(&file);
    QStringList allLines;
    while (!in.atEnd()) {
        allLines.append(in.readLine());
    }
    file.close();

    // 校验行数必须一致（否则会错位）
    int existingDataRows = allLines.size() - 1; // 减去表头行
    if (existingDataRows != rowCount) {
        qDebug() << "错误：新数据行数与已有数据行数不匹配！";
        return 0;
    }

    // 重新写入文件：在每一行末尾追加新列
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件写入：" << m_csvFilePath;
        return 0;
    }

    QTextStream out(&file);
    //out.setCodec("UTF-8");

    // ① 追加表头：在原表头后加新的5列表头
    allLines[0] += ",时间,通道,质量,波长,原始光谱,解析光谱";
    out << allLines[0] << "\n";

    // ② 追加数据：在每一行数据后加新的6列数据
    for (int row = 0; row < rowCount; ++row) {
        QString newPart;
        if (row == 0) {
            // 首行：时间+通道+光谱数据
            newPart = QString(",%1,%2,%3,%4,%5,%6")
                          .arg(timeStr)
                          .arg(channel)
                          .arg(quality)
                          .arg(waveLength[row])
                          .arg(originalSpectrum[row])
                          .arg(analyzeSpectrum[row]);
        } else {
            // 非首行：时间+通道留空，只加光谱数据
            newPart = QString(",,,,%1,%2,%3")
                          .arg(waveLength[row])
                          .arg(originalSpectrum[row])
                          .arg(analyzeSpectrum[row]);
        }
        out << allLines[row + 1] + newPart << "\n";
    }

    file.close();
    return 1;

}
