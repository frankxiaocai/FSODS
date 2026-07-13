#include "devicemanager.h"

DeviceManager::DeviceManager(QObject *parent)
    : QObject{parent}
    ,m_HikCamera(new HikCamera(this))
    ,m_HyperspectralCamera(new HyperspectralCamera(this))
    ,m_larmanModbusTCP(new LarmanModbusTCP(this))
    ,m_siemensModbusPlc(new PlcController(this))
{
    init();
}

DeviceManager::~DeviceManager()
{
    qDebug() << "DeviceManager 析构释放";
}

void DeviceManager::init()
{
    //相机采集信号
    connect(m_HikCamera, &HikCamera::sig_newImage, this, &DeviceManager::sig_newImage);
    connect(m_HikCamera, &HikCamera::sig_autoCaptured, this, &DeviceManager::sig_autoCaptured);
    // 高光谱采集信号
    connect(m_HyperspectralCamera, &HyperspectralCamera::sig_batchFinished,this,&DeviceManager::sig_batchFinished);
    connect(m_HyperspectralCamera, &HyperspectralCamera::sig_batchFinished,this,&DeviceManager::slot_onFrameArrived);
    //电控
    connect(m_siemensModbusPlc, &PlcController::sig_regChanged, this, &DeviceManager::slot_onWasteArrived);
}

Error_code DeviceManager::initEleControl()
{
    bool error = m_siemensModbusPlc->plcconnect("192.168.0.140",501);
    if(!error)
    {
        LOG_INFO("电控连接失败");
        return Error_EleControl;
    }
    LOG_INFO("电控初始化成功");
    m_siemensModbusPlc->startReadReg();
    return Error_None;
}

Error_code DeviceManager::initCamera()
{
    // 枚举设备
    if (!m_HikCamera->enumDevices())
    {
        return Error_Camera;
    }

    // 打开第一个设备（index=0）
    if (!m_HikCamera->openDevice(0))
    {
        return Error_Camera;
    }

    // 开启【传送带自动抓图】
    m_HikCamera->enableAutoCapture(true);

    // 启动采集
    bool sg = m_HikCamera->startGrabbing();
    if(!sg){return Error_Camera;}

    LOG_INFO("相机初始化成功");
    return Error_None;
}

Error_code DeviceManager::initLumo()
{
    // 配置文件
    int SetSSPerr = m_HyperspectralCamera->setSSP();
    if(SetSSPerr!= 0){return Error_Hyperspectral;}

    // 加载→打开→初始化
    bool loadSDKerr = m_HyperspectralCamera->loadSDK();
    if(!loadSDKerr){return Error_Hyperspectral;}

    bool openDeviceerr = m_HyperspectralCamera->openDevice(0);
    if(!openDeviceerr){return Error_Hyperspectral;}

    bool initializeerr = m_HyperspectralCamera->initialize();
    if(!initializeerr){return Error_Hyperspectral;}

    // //设置相机参数
    // m_HyperspectralCamera->setExposure(m_Exposure);//曝光时间 ms
    // m_HyperspectralCamera->setFrameRate(m_FrameRate);//帧率

    LOG_INFO("Lumo初始化成功");
    return Error_None;
}

Error_code DeviceManager::initLarman()
{
    Error_code err = m_larmanModbusTCP->connectToDevice("192.168.3.30");
    if(err!=Error_None)
    {
        LOG_INFO("拉曼初始化失败");
        return err;
    }
    LOG_INFO("拉曼初始化成功");
    return Error_None;
}

Error_code DeviceManager::lumoCapture(int XNum)
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("高光谱 开始采集时间：" + currentTime);
    // 设置采集 X 行自动停
    m_HyperspectralCamera->setAcquireLineCount(XNum);
    bool error = m_HyperspectralCamera->startAcquisition();
    if(!error)
    {
        LOG_ERROR("lumo 采集高光谱失败");
        return Error_Hyperspectral;
    }
    return Error_None;
}

Error_code DeviceManager::larmanCapture()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("拉曼 开始采集时间：" + currentTime);
    //开始采集
    Error_code temp_error = m_larmanModbusTCP->startSpectrumCollect();
    if(temp_error!=Error_None)
    {
        LOG_ERROR("拉曼 ModbusTCP采集光谱失败");
        return temp_error;
    }

    while (true)
    {
        // 循环读取光谱仪采集状态
        SpectrometerStatus status;
        Error_code err = m_larmanModbusTCP->getCollectStatus(status);
        if (err != Error_None) {return err;}

        // 判断：状态为 Normal → 跳出循环
        if (status == Status_Normal)
        {
            break;
        }
    }
    //读取波长
    QVector<float> temp_wave;
    temp_error = m_larmanModbusTCP->getWavelengthData(temp_wave);
    if(temp_error!=Error_None)
    {
        LOG_ERROR("拉曼 读取拉曼波长失败");
        return temp_error;
    }
    //读取原始光谱
    QVector<float> temp_originalSpectrum;
    temp_error = m_larmanModbusTCP->getOriginalSpectrum(temp_originalSpectrum);
    if(temp_error!=Error_None)
    {
        LOG_ERROR("拉曼 读取原始光谱失败");
        return temp_error;
    }
    QString currentTime2 = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("拉曼 结束采集&算法开始识别时间：" + currentTime2);
    int type = 0;
    //m_RamanPlasticRecognizer.setTrainDirectory("E:/train_csv");
    QString trainDir = QDir(QCoreApplication::applicationDirPath()).filePath("train_csv");
    m_RamanPlasticRecognizer.setTrainDirectory(trainDir.toStdString());
    std::vector<float> std_wave(temp_wave.cbegin(), temp_wave.cend());
    std::vector<float> std_originalSpectrum(temp_originalSpectrum.cbegin(), temp_originalSpectrum.cend());
    RamanErrorCode error = m_RamanPlasticRecognizer.recognition(std_wave,std_originalSpectrum,type);
    if(error!=Error_None_raman)
    {
        qDebug()<<"拉曼塑料算法识别失败:  "<<error;
        LOG_ERROR("拉曼塑料算法识别失败");
        return Error_Larman;
    }
    QString currentTime3 = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("拉曼 算法识别结束时间：" + currentTime3);
    emit sig_plasticType(type);
    return Error_None;

}

void DeviceManager::setExposure(double aaa)
{
    m_Exposure = aaa;
    //设置相机参数
    m_HyperspectralCamera->setExposure(m_Exposure);//曝光时间 ms
}

void DeviceManager::setFrameRate(double aaa)
{
    m_FrameRate = aaa;
    //设置相机参数
    m_HyperspectralCamera->setFrameRate(m_FrameRate);//帧率
}

void DeviceManager::beltOpen(int num, bool isopen)
{
    m_siemensModbusPlc->beltControl(num,isopen);
}

void DeviceManager::beltSpeed(int num, int speed)
{
    m_siemensModbusPlc->beltSpeedControl(num,speed);
}

void DeviceManager::pushControl(int num,bool op)
{
    m_siemensModbusPlc->pushControltest(num,op);
}

void DeviceManager::turnControl(int order)
{
    if(order == 0)
    {
        m_siemensModbusPlc->turnControl(0);
    }
    else if(order == 1)
    {
        m_siemensModbusPlc->turnControl(1);
    }
    else if(order == 2)
    {
        m_siemensModbusPlc->zuoControl();
    }
    else if(order == 3)
    {
        m_siemensModbusPlc->zuoControl2();
    }
    else if(order == 4)
    {
        m_siemensModbusPlc->youControl();
    }
    else if(order == 5)
    {
        m_siemensModbusPlc->youControl2();
    }
}

void DeviceManager::updateObjectCount(int objType)
{
    m_objCount[objType]++;
    m_objTotal++;
}

int DeviceManager::getObjTypeCount(int type)
{
    return m_objCount[type];
}

int DeviceManager::getObjTotalCount()
{
    return m_objTotal;
}

void DeviceManager::clearAllObjectCount()
{
    for(int i=0;i<=7;i++) m_objCount[i]=0;
    m_objTotal=0;
}

void DeviceManager::testcount()
{
    int type = QRandomGenerator::global()->bounded(8);
    emit sig_plasticType(type);
}

void DeviceManager::writeBatch2Raw(const HyperLineBatch &batch,int type)
{
    QString timeStr = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString saveDir = "E:/test";
    QString rawFileName = QString("hyperspec_%1.raw").arg(timeStr);
    QString txtFileName = QString("hyperspec_info.txt");

    QString rawFilePath = QDir(saveDir).filePath(rawFileName);
    QString txtFilePath = QDir(saveDir).filePath(txtFileName);

    // ===================== 写入 RAW 二进制文件 =====================
    QFile rawFile(rawFilePath);
    if (!rawFile.open(QIODevice::WriteOnly))
    {
        LOG_ERROR(QString("高光谱raw文件打开失败：%1").arg(rawFilePath));
        return;
    }

    const char* rawPtr = reinterpret_cast<const char*>(batch.data.data());
    qint64 totalByte = batch.data.size();
    rawFile.write(rawPtr, totalByte);
    rawFile.close();


    // ===================== 写入配套 TXT 信息文件 =====================
    QFile txtFile(txtFilePath);
    if (!txtFile.open(QIODevice::Append | QIODevice::Text))
    {
        LOG_ERROR(QString("高光谱txt文件打开失败：%1").arg(txtFilePath));
        return;
    }

    QString txtContent = QString(
                             "采集时间戳：%1\n"
                             "采集类型type：%2\n"
                             ).arg(timeStr).arg(type);

    txtFile.write(txtContent.toUtf8());
    txtFile.close();

}

void DeviceManager::slot_onFrameArrived(const HyperLineBatch &batch)
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("高光谱 结束采集&算法开始识别时间：" + currentTime);
    //算法分类
    int type = 0;
    error_code_HSI errorHSI = m_HSIClassifier.classifyFinalLabel(batch,type);
    if(errorHSI!= Error_None_HSI)
    {
        LOG_INFO("高光谱 塑料识别算法失败");
        return;
    }
    QString currentTime2 = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("高光谱 算法识别结束时间：" + currentTime2);
    emit sig_plasticType(type);

    //执行制动
    switch (type)
    {
    case 7:
        QTimer::singleShot(m_delayMsL1, this, [=]() {
            m_siemensModbusPlc->pushControltest(1, true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->pushControltest(1, false);
            });
        });
        break;

    case 5:
        QTimer::singleShot(m_delayMsL2, this, [=]() {
            m_siemensModbusPlc->pushControltest(2, true);

            QTimer::singleShot(1500, this, [=]() {
                m_siemensModbusPlc->pushControltest(2, false);
            });
        });
        break;

    case 3:
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            m_siemensModbusPlc->zuoControl();

            QTimer::singleShot(2000, this, [=]() {
                m_siemensModbusPlc->zuoControl2();
            });
        });
        break;

    case 2:
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            m_siemensModbusPlc->youControl();

            QTimer::singleShot(2000, this, [=]() {
                m_siemensModbusPlc->youControl2();
            });
        });
        break;

    default:
        // type不匹配任何值时的处理
        if(m_isSave)
        {
            writeBatch2Raw(batch,type);
        }
        break;
    }
}

void DeviceManager::slot_onWasteArrived()
{
    //物体到达信号 执行采集
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("光栅 物体来了" + currentTime);
    lumoCapture(m_XLines);
}
