#include "fileio.h"
FileIO* FileIO::m_instance = nullptr;
FileIO::AutoDelete FileIO::m_autoDelete;
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


bool FileIO::writeConfig(const QVector<scanConfigs> &configVec, const otherConfigs &otherCfg)
{

    if (configVec.isEmpty()) {
        qDebug() << "配置数组为空，写入失败";
        return false;
    }

    QSettings settings(m_configPath, QSettings::IniFormat);

    // ===================== 写入 scanConfigs 数组（3个元素）=====================
    for (int i = 0; i < configVec.size(); ++i) {
        const scanConfigs& cfg = configVec[i];
        // 分组名：ScanConfig_
        QString group = QString("ScanConfig_%1").arg(i);

        settings.beginGroup(group);
        settings.setValue("wtime", cfg.wtime);
        settings.setValue("ctime", cfg.ctime);
        settings.setValue("disment_X", cfg.disment_X);
        settings.setValue("disement_Z", cfg.disement_Z);
        settings.setValue("zero_disment_X", cfg.zero_disment_X);
        settings.setValue("zero_disment_Z", cfg.zero_disment_Z);
        settings.setValue("areascan_numpoint", cfg.areascan_numpoint);
        settings.setValue("areascan_disment", cfg.areascan_disment);
        settings.setValue("laser_power", cfg.laser_power);
        settings.setValue("laser_time", cfg.laser_time);
        settings.setValue("laser_frequency", cfg.laser_frequency);
        settings.endGroup();
    }

    // ===================== 写入 otherConfigs 独立配置 =====================
    settings.beginGroup("OtherConfig");
    settings.setValue("m_timeout", otherCfg.m_timeout);
    settings.endGroup();

    return true;
}

bool FileIO::readConfig(QVector<scanConfigs> &configVec)
{
    // 清空旧数据
    configVec.clear();
    QSettings settings(m_configPath, QSettings::IniFormat);
    //settings.setIniCodec("UTF-8");

    // 检查配置文件是否存在
    if (!QFile::exists(m_configPath)) {
        qDebug() << "配置文件不存在，读取失败";
        return false;
    }

    // ===================== 读取 scanConfigs 数组（固定3个）=====================
    const int configCount = 3;
    for (int i = 0; i < configCount; ++i) {
        scanConfigs cfg;
        QString group = QString("ScanConfig_%1").arg(i);

        settings.beginGroup(group);
        cfg.wtime = settings.value("wtime", cfg.wtime).toInt();
        cfg.ctime = settings.value("ctime", cfg.ctime).toInt();
        cfg.disment_X = settings.value("disment_X", cfg.disment_X).toFloat();
        cfg.disement_Z = settings.value("disement_Z", cfg.disement_Z).toFloat();
        cfg.zero_disment_X = settings.value("zero_disment_X", cfg.zero_disment_X).toFloat();
        cfg.zero_disment_Z = settings.value("zero_disment_Z", cfg.zero_disment_Z).toFloat();
        cfg.areascan_numpoint = settings.value("areascan_numpoint", cfg.areascan_numpoint).toInt();
        cfg.areascan_disment = settings.value("areascan_disment", cfg.areascan_disment).toFloat();
        cfg.laser_power = settings.value("laser_power", cfg.laser_power).toInt();
        cfg.laser_time = settings.value("laser_time", cfg.laser_time).toInt();
        cfg.laser_frequency = settings.value("laser_frequency", cfg.laser_frequency).toInt();
        settings.endGroup();

        configVec.append(cfg);
    }

    return true;
}

bool FileIO::readConfig(otherConfigs &otherCfg)
{
    // 清空旧数据
    QSettings settings(m_configPath, QSettings::IniFormat);

    // 检查配置文件是否存在
    if (!QFile::exists(m_configPath)) {
        qDebug() << "配置文件不存在，读取失败";
        return false;
    }

    // ===================== 读取 otherConfigs 独立配置 =====================
    settings.beginGroup("OtherConfig");
    otherCfg.m_timeout = settings.value("m_timeout", otherCfg.m_timeout).toInt();
    otherCfg.Server1IP = QHostAddress(settings.value("Server1IP", "127.0.0.1").toString());
    otherCfg.Server1Port = settings.value("Server1Port", 502).toInt();
    otherCfg.Server2IP = QHostAddress(settings.value("Server2IP", "127.0.0.1").toString());
    otherCfg.Server2Port = settings.value("Server2Port", 503).toInt();
    otherCfg.spacetime = settings.value("spacetime", 10).toInt();
    otherCfg.burner1 = settings.value("burner1", "燃烧器").toString();
    otherCfg.burner2 = settings.value("burner2", "燃烧器").toString();
    otherCfg.burner3 = settings.value("burner3", "燃烧器").toString();
    otherCfg.path1 = settings.value("path1", "D:/SSP").toString();
    otherCfg.path2 = settings.value("path2", "D:/SSP").toString();
    otherCfg.path3 = settings.value("path3", "D:/SSP").toString();
    settings.endGroup();

    return true;
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