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
    //====线程安全退出顺序：quit → wait → delete对象====
    m_workerThread->quit();
    m_workerThread->wait();

    delete m_modbusWorker;
    delete m_workerThread;

    qDebug() << "DeviceManager 析构释放";
}

void DeviceManager::init()
{
    //相机采集信号
    connect(m_HikCamera, &HikCamera::sig_newImage, this, &DeviceManager::sig_newImage);
    connect(m_HikCamera, &HikCamera::sig_objectCapture, this, &DeviceManager::slot_onHikCaptureArrived);
    connect(m_HikCamera, &HikCamera::sig_objectLocation, this, &DeviceManager::sig_hikObjectXY);
    connect(m_HikCamera, &HikCamera::sig_objectLocation, this, &DeviceManager::slot_hikObjectXY);
    // 高光谱采集信号
    connect(m_HyperspectralCamera, &HyperspectralCamera::sig_batchFinished,this,&DeviceManager::sig_batchFinished);
    connect(m_HyperspectralCamera, &HyperspectralCamera::sig_batchFinished,this,&DeviceManager::slot_onFrameArrived);
    //光栅
    // connect(m_siemensModbusPlc, &PlcController::sig_regChanged, this, &DeviceManager::slot_onObjectArrived);
    // connect(m_siemensModbusPlc, &PlcController::sig_guangshanValue, this, &DeviceManager::sig_guangshanValue);
    //制动
    connect(this, &DeviceManager::sig_plasticType_hsi, this, &DeviceManager::slot_actControl);
    connect(this, &DeviceManager::sig_plasticType_larman, this, &DeviceManager::slot_lamanActControl);
    //运动轴
    // connect(m_siemensModbusPlc, &PlcController::sig_regBeltStop, this, &DeviceManager::slot_larZhou_beltStop);
    // connect(m_siemensModbusPlc, &PlcController::sig_regFocusON, this, &DeviceManager::slot_larZhou_focusOn);

}

Error_code DeviceManager::initEleControl()
{
    // //====旧版本ModbusPlc类
    // bool error = m_siemensModbusPlc->plcconnect("192.168.0.140",501);
    // if(!error)
    // {
    //     LOG_INFO("电控连接失败");
    //     return Error_EleControl;
    // }
    // LOG_INFO("电控初始化成功");
    // m_siemensModbusPlc->startReadReg();
    // return Error_None;

    //====【改动】new ModbusWorker() 不要传this，禁止父对象====
    m_modbusWorker = new ModbusWorker();
    m_workerThread = new QThread;
    m_modbusWorker->moveToThread(m_workerThread);
    m_workerThread->start();

    connect(m_modbusWorker,&ModbusWorker::sig_logMsg,this,[](const QString& s){
        LOG_INFO(s);
    });

    connect(m_modbusWorker,&ModbusWorker::sig_urgentWriteFinished,this,[](bool ok,QString tag,quint64 sub,quint64 done){

        QString logStr = QString("紧急请求ok？=%1 紧急请求内容=%2 发送请求时间戳:%3 发送完成反馈时间戳:%4")
                             .arg(ok)
                             .arg(tag)
                             .arg(sub)
                             .arg(done);
        LOG_INFO(logStr);

    });

    connect(m_modbusWorker, &ModbusWorker::sig_pollReadDone, this, &DeviceManager::slot_pollReadDone);

    // m_modbusWorker->plcconnect("192.168.0.140",501);
    // m_modbusWorker->startPoll(200);
    QMetaObject::invokeMethod(m_modbusWorker, "plcconnect",
                              Qt::QueuedConnection,
                              Q_ARG(QString, "192.168.0.140"),
                              Q_ARG(quint16, 501));
    QMetaObject::invokeMethod(m_modbusWorker, "startPoll",
                              Qt::QueuedConnection,
                              Q_ARG(int, 200));
    LOG_INFO("电控初始化成功");
    return Error_None;
}

void DeviceManager::HIKCapture()
{
    m_HikCamera->hikOnceCapture();
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
    // 采集 X 行
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
    LOG_INFO("拉曼 光谱仪采集状态为1");
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
    LOG_INFO("拉曼 算法开始识别时间：" + currentTime2);
    int type = 0;
    m_RamanPlasticRecognizer.setTrainDirectory("E:/train_csv");
    //QString trainDir = QDir(QCoreApplication::applicationDirPath()).filePath("train_csv");
    //m_RamanPlasticRecognizer.setTrainDirectory(trainDir.toStdString());
    std::vector<float> std_wave(temp_wave.cbegin(), temp_wave.cend());
    std::vector<float> std_originalSpectrum(temp_originalSpectrum.cbegin(), temp_originalSpectrum.cend());
    RamanErrorCode error = m_RamanPlasticRecognizer.recognition(std_wave,std_originalSpectrum,type);
    if(error!=Error_None_raman)
    {
        LOG_ERROR("拉曼塑料算法识别失败");
        return Error_Larman;
    }
    QString currentTime3 = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("拉曼 算法识别结束时间：" + currentTime3);

    emit sig_plasticType_larman(type);
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

void DeviceManager::slot_actControl(int type)
{
    //执行制动
    switch (type)
    {
    //拨杆
    case 3:
        QTimer::singleShot(m_delayMsL1, this, [=]() {
            //m_siemensModbusPlc->pushOnOff(1, true);
            pushControl(1, true);

            QTimer::singleShot(1000, this, [=]() {
                //m_siemensModbusPlc->pushOnOff(1, false);
                pushControl(1, false);
            });
        });
        break;
        //推杆
    case 2:
        QTimer::singleShot(m_delayMsL2, this, [=]() {
            //m_siemensModbusPlc->pushOnOff(2, true);
            pushControl(2, true);

            QTimer::singleShot(1500, this, [=]() {
                //m_siemensModbusPlc->pushOnOff(2, false);
                pushControl(2, false);
            });
        });
        break;
        //万向轮1 左
    case 7:
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            //m_siemensModbusPlc->turnZuo(1,true);
            turnControl(1,2);

            QTimer::singleShot(1000, this, [=]() {
                //m_siemensModbusPlc->turnZuo(1,false);
                turnControl(1,3);
            });
        });
        break;
        //万向轮1 右
    case 4:
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            //m_siemensModbusPlc->turnYou(1,true);
            turnControl(1,4);

            QTimer::singleShot(1000, this, [=]() {
                //m_siemensModbusPlc->turnYou(1,false);
                turnControl(1,5);
            });
        });
        break;
        //万向轮2 左
    case 5:
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            //m_siemensModbusPlc->turnZuo(2,true);
            turnControl(2,2);

            QTimer::singleShot(1000, this, [=]() {
                //m_siemensModbusPlc->turnZuo(2,false);
                turnControl(2,3);
            });
        });
        break;
        //万向轮2 右
    case 6:
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            //m_siemensModbusPlc->turnYou(2,true);
            turnControl(2,4);

            QTimer::singleShot(1000, this, [=]() {
                //m_siemensModbusPlc->turnYou(2,false);
                turnControl(2,5);
            });
        });
        break;

    }
}

void DeviceManager::wheelActControl(int type)
{
    if(type == m_lastMaterial)
    {
        qDebug()<<"相同物料 无操作";
    }
    else
    {
        //前一物料万向轮归正
        int lastMaterial_T1 = getT1(m_lastMaterial);
        int lastMaterial_T2 = getT2(m_lastMaterial);
        QTimer::singleShot(lastMaterial_T1+lastMaterial_T2, this, [=]() {
            wheelReset(m_lastMaterial);
        });

        //延迟T1 执行当前类型物料动作
        int T1 = getT1(type);
        QTimer::singleShot(T1, this, [=]() {
            wheelAct(type);
        });

    }
    // 记录本次的物料类型，作为下一次对比的基准
    m_lastMaterial = type;
}

void DeviceManager::slot_actControl_new2(int type)
{
    if(type == shift_type)//拨杆
    {
        QTimer::singleShot(m_delayMsL1, this, [=]() {
            pushControl(1, true);

            QTimer::singleShot(1000, this, [=]() {
                pushControl(1, false);
            });
        });
    }
    else if(type == push_type)//推杆
    {
        QTimer::singleShot(m_delayMsL2, this, [=]() {
            pushControl(2, true);

            QTimer::singleShot(1500, this, [=]() {
                pushControl(2, false);
            });
        });
    }
    else//万向轮
    {
        wheelActControl(type);
    }
}


void DeviceManager::slot_larZhou_beltStop()
{
    //皮带静止
    LOG_INFO("接收来自拉曼PLC 皮带停止信号");
}

void DeviceManager::slot_larZhou_focusOn()
{
    LOG_INFO("接收来自拉曼PLC 聚焦完成信号");

    //larmanCapture();

}

void DeviceManager::slot_pollReadDone(int regAddr, quint16 val)
{
    if(regAddr == m_adress_grating)//光栅轮询
    {
        emit sig_guangshanValue(val);
        // 对比上次值，发生变化则触发信号+日志
        if ((val != m_lastRegVal)&(val == 1))
        {
            slot_onObjectArrived();
        }
        m_lastRegVal = val;
    }
    else if(regAddr == m_adress_LarZhou_focusOn)//聚焦完成轮询
    {

        if(val == 1)
        {
            //启动拉曼
        }

    }
    else
    {

    }
}

void DeviceManager::beltOpen(int num, bool isopen)
{
    //m_siemensModbusPlc->beltOnOff(num,isopen);
    int value = isopen ? 1 : 0;
    quint16 addr;

    switch (num)
    {
    case 1:
        addr = m_adress_belt1OI;
        break;
    case 2:
        addr = m_adress_belt2OI;
        break;
    case 3:
        addr = m_adress_belt3OI;
        break;
    case 4:
        addr = m_adress_belt4OI;
        break;
    case 5:
        addr = m_adress_belt5OI;
        break;
    case 6:
        addr = m_adress_belt6OI;
        break;
    case 7:
        addr = m_adress_belt7OI;
        break;
    case 8:
        addr = m_adress_belt8OI;
        break;
    case 9:
        addr = m_adress_belt9OI;
        break;
    default:
        qWarning() << "beltOpen: 无效皮带编号 num=" << num;
        return;
    }

    emit m_modbusWorker->sigUrgentWrite(addr, value, QString("皮带%1启停").arg(num));
}

void DeviceManager::beltSpeed(int num, int speed)
{
    //m_siemensModbusPlc->beltSpeedControl(num,speed);
    quint16 addr;

    switch (num)
    {
    case 1:
        addr = m_adress_belt1Speed;
        break;
    case 2:
        addr = m_adress_belt2Speed;
        break;
    case 3:
        addr = m_adress_belt3Speed;
        break;
    case 4:
        addr = m_adress_belt4Speed;
        break;
    case 5:
        addr = m_adress_belt5Speed;
        break;
    case 6:
        addr = m_adress_belt6Speed;
        break;
    case 7:
        addr = m_adress_belt7Speed;
        break;
    case 8:
        addr = m_adress_belt8Speed;
        break;
    case 9:
        addr = m_adress_belt9Speed;
        break;
    default:
        qWarning() << "beltOpen: 无效皮带编号 num=" << num;
        return;
    }

    emit m_modbusWorker->sigUrgentWrite(addr, speed, QString("皮带%1速度").arg(num));
}

void DeviceManager::pushControl(int num,bool op)
{
    //m_siemensModbusPlc->pushOnOff(num,op);
    int value = op ? 1 : 0;
    quint16 addr;

    switch (num)
    {
    case 1:
        addr = m_adress_shiftOI;
        break;
    case 2:
        addr = m_adress_pushOI;
        break;
    default:
        qWarning() << "beltOpen: 无效皮带编号 num=" << num;
        return;
    }

    emit m_modbusWorker->sigUrgentWrite(addr, value, QString("推拨杆%1启停").arg(num));
}

void DeviceManager::turnControl(int num,int order)
{
    quint16 addr01 =  m_adress_wheel1OI;
    if(num == 2)
    {
        addr01 =  m_adress_wheel2OI;
    }

    quint16 addr23 =  m_adress_wheel1_left;
    if(num == 2)
    {
        addr23 =  m_adress_wheel2_left;
    }

    quint16 addr45 =  m_adress_wheel1_right;
    if(num == 2)
    {
        addr45 =  m_adress_wheel2_right;
    }

    if(order == 0)//万向轮关
    {
        //m_siemensModbusPlc->turnOnOff(num,false);
        emit m_modbusWorker->sigUrgentWrite(addr01, 0, QString("万向轮%1停止").arg(num));
    }
    else if(order == 1)//万向轮开
    {
        //m_siemensModbusPlc->turnOnOff(num,true);
        emit m_modbusWorker->sigUrgentWrite(addr01, 1, QString("万向轮%1启动").arg(num));
    }
    else if(order == 2)//万向轮左转
    {
        //m_siemensModbusPlc->turnZuo(num,true);
        emit m_modbusWorker->sigUrgentWrite(addr23, 1, QString("万向轮%1左转").arg(num));
    }
    else if(order == 3)//万向轮左转回正
    {
        //m_siemensModbusPlc->turnZuo(num,false);
        emit m_modbusWorker->sigUrgentWrite(addr23, 0, QString("万向轮%1左转回正").arg(num));
    }
    else if(order == 4)//万向轮右转
    {
        //m_siemensModbusPlc->turnYou(num,true);
        emit m_modbusWorker->sigUrgentWrite(addr45, 1, QString("万向轮%1右转").arg(num));
    }
    else if(order == 5)//万向轮右转回正
    {
        //m_siemensModbusPlc->turnYou(num,false);
        emit m_modbusWorker->sigUrgentWrite(addr45, 0, QString("万向轮%1右转回正").arg(num));
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

void DeviceManager::setLarZhouOI(bool isok)
{
    if(isok)
    {
        //m_siemensModbusPlc->setLarZhouStart();
        emit m_modbusWorker->sigUrgentWrite(m_adress_larZhouOI, 1, "允许对焦");
    }
    else {
        //m_siemensModbusPlc->setLarZhouStop();
        emit m_modbusWorker->sigUrgentWrite(m_adress_larZhouOI, 0, "不允许对焦");
    }
}


void DeviceManager::test()
{

    std::string imgPath = "E:/test/555.jpeg";
    cv::Mat srcMat = cv::imread(imgPath, cv::IMREAD_COLOR);
    if (srcMat.empty())
    {
        qDebug() << "图像读取失败";
        return;
    }
    cv::Mat m_grayMat,m_mergeMat,m_grayDrawMat;
    double X,Y;
    cv::cvtColor(srcMat, m_grayMat, cv::COLOR_BGR2GRAY);
    m_HikCamera->objectLocate(m_grayMat,m_mergeMat,X,Y,m_grayDrawMat);

    // 弹窗显示灰度图
    cv::imshow("Draw Image", m_grayDrawMat);
    slot_onHikCaptureArrived(m_grayDrawMat);

    emit sig_hikObjectXY(X,Y); //物体定位
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

QImage DeviceManager::Mat2QImage(const cv::Mat &mat)
{
    if (mat.empty())
        return QImage();

    switch (mat.type())
    {
    // 单通道灰度图 CV_8UC1
    case CV_8UC1:
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
    // 三通道BGR图 CV_8UC3（绘图后的彩色图）
    case CV_8UC3:
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888);
    default:
        return QImage();
    }
}

void DeviceManager::slot_lamanActControl(int type)
{

}

void DeviceManager::execW1IDLE()
{
    if(m_curW1State == WheelRealState::LEFT)
    {
        // 左转回正
        //m_siemensModbusPlc->turnZuo(1,false);
        turnControl(1,3);
    }

    if(m_curW1State == WheelRealState::RIGHT)
    {
        // 左转回正
        //m_siemensModbusPlc->turnYou(1,false);
        turnControl(1,5);
    }

    m_curW1State = WheelRealState::IDLE;
}

void DeviceManager::execW2IDLE()
{

    if(m_curW2State == WheelRealState::LEFT)
    {
        // 左转回正
        //m_siemensModbusPlc->turnZuo(2,false);
        turnControl(2,3);
    }

    if(m_curW2State == WheelRealState::RIGHT)
    {
        // 右转回正
        //m_siemensModbusPlc->turnYou(2,false);
        turnControl(2,5);
    }

    m_curW2State = WheelRealState::IDLE;
}

void DeviceManager::wheelAct(int type)
{
    if(type == wheel1_left_type)//1号轮 左转
    {
        turnControl(1,2);
        m_curW1State = WheelRealState::LEFT;
    }
    else if(type == wheel1_right_type)//1号轮 右转
    {
        turnControl(1,4);
        m_curW1State = WheelRealState::RIGHT;
    }
    else if(type == wheel2_left_type)//2号轮 左转
    {
        turnControl(2,2);
        m_curW2State = WheelRealState::LEFT;
    }
    else if(type == wheel2_right_type)//2号轮 右转
    {
        turnControl(2,4);
        m_curW2State = WheelRealState::RIGHT;
    }
    else if(type == 8)//1、2号轮回正
    {
        execW1IDLE();
        QTimer::singleShot(200, this, [=]() {
            execW2IDLE();
        });
    }
    else
    {
        //
    }
}

void DeviceManager::wheelReset(int type)
{
    if(type == wheel1_left_type)//1号轮 左转归正
    {
        turnControl(1,3);
        m_curW1State = WheelRealState::IDLE;
    }
    else if(type == wheel1_right_type)//1号轮 右转归正
    {
        turnControl(1,5);
        m_curW1State = WheelRealState::IDLE;
    }
    else if(type == wheel2_left_type)//2号轮 左转归正
    {
        turnControl(2,3);
        m_curW2State = WheelRealState::IDLE;
    }
    else if(type == wheel2_right_type)//2号轮 右转归正
    {
        turnControl(2,5);
        m_curW2State = WheelRealState::IDLE;
    }
    else if(type == 8)//1、2号轮回正
    {
        execW1IDLE();
        QTimer::singleShot(200, this, [=]() {
            execW2IDLE();
        });

    }
    else{}
}

int DeviceManager::getT1(int type)
{
    int T1 = 0;
    if(type == wheel1_left_type ||type == wheel1_right_type)
    {
        T1 = m_delayMsL3;
    }
    else if(type == wheel2_left_type ||type == wheel2_right_type)
    {
        T1 = m_delayMsL4;
    }
    else if(type == shift_type)
    {
        T1 = m_delayMsL1;
    }
    else if(type == push_type)
    {
        T1 = m_delayMsL2;
    }
    else{}
    return T1;
}

int DeviceManager::getT2(int type)
{
    int T2 = 1000;
    return T2;
}

void DeviceManager::slot_onFrameArrived(const HyperLineBatch &batch)
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("高光谱 算法开始识别时间：" + currentTime);
    //算法分类
    int type = 0;
    error_code_HSI errorHSI = m_HSIClassifier.classifyFinalLabel(batch,type);
    if(errorHSI!= Error_None_HSI)
    {
        LOG_INFO("高光谱 塑料识别算法失败");
        return;
    }
    QString currentTime2 = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("高光谱 制动指令开始时间：" + currentTime2);
    emit sig_plasticType_hsi(type);

    //保存光谱信息
    if(m_isSave)
    {
        writeBatch2Raw(batch,type);
    }

}

void DeviceManager::slot_onHikCaptureArrived(cv::Mat targetOnly)
{
    QImage imgTarget = Mat2QImage(targetOnly);
    emit sig_hikCaptured(imgTarget);
}

void DeviceManager::slot_hikObjectXY(double X, double Y)
{
    //传输给拉曼运动轴
}

void DeviceManager::slot_onObjectArrived()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("光栅 识别到物体" + currentTime);
    lumoCapture(m_XLines);
}
