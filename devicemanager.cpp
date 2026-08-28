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
    connect(m_HikCamera, &HikCamera::sig_objectCapture, this, &DeviceManager::slot_onHikCaptureArrived);
    connect(m_HikCamera, &HikCamera::sig_objectLocation, this, &DeviceManager::sig_hikObjectXY);
    connect(m_HikCamera, &HikCamera::sig_objectLocation, this, &DeviceManager::slot_hikObjectXY);
    // 高光谱采集信号
    connect(m_HyperspectralCamera, &HyperspectralCamera::sig_batchFinished,this,&DeviceManager::sig_batchFinished);
    connect(m_HyperspectralCamera, &HyperspectralCamera::sig_batchFinished,this,&DeviceManager::slot_onFrameArrived);
    //光栅
    connect(m_siemensModbusPlc, &PlcController::sig_regChanged, this, &DeviceManager::slot_onObjectArrived);
    connect(m_siemensModbusPlc, &PlcController::sig_guangshanValue, this, &DeviceManager::sig_guangshanValue);
    //制动
    connect(this, &DeviceManager::sig_plasticType, this, &DeviceManager::slot_actControl);
    //运动轴
    connect(m_siemensModbusPlc, &PlcController::sig_regBeltStop, this, &DeviceManager::slot_larZhou_beltStop);
    connect(m_siemensModbusPlc, &PlcController::sig_regFocusON, this, &DeviceManager::slot_larZhou_focusOn);

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
    m_RamanPlasticRecognizer.setTrainDirectory("E:/train_csv");
    //QString trainDir = QDir(QCoreApplication::applicationDirPath()).filePath("train_csv");
    //m_RamanPlasticRecognizer.setTrainDirectory(trainDir.toStdString());
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

    emit sig_plasticType_larman(type);
    lamanActControl(type);
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
            m_siemensModbusPlc->pushOnOff(1, true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->pushOnOff(1, false);
            });
        });
        break;
//推杆
    case 2:
        QTimer::singleShot(m_delayMsL2, this, [=]() {
            m_siemensModbusPlc->pushOnOff(2, true);

            QTimer::singleShot(1500, this, [=]() {
                m_siemensModbusPlc->pushOnOff(2, false);
            });
        });
        break;
//万向轮1 左
    case 7:
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            m_siemensModbusPlc->turnZuo(1,true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->turnZuo(1,false);
            });
        });
        break;
//万向轮1 右
    case 4:
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            m_siemensModbusPlc->turnYou(1,true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->turnYou(1,false);
            });
        });
        break;
//万向轮2 左
    case 5:
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            m_siemensModbusPlc->turnZuo(2,true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->turnZuo(2,false);
            });
        });
        break;
//万向轮2 右
    case 6:
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            m_siemensModbusPlc->turnYou(2,true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->turnYou(2,false);
            });
        });
        break;

    }
    // QString currentTime3 = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    // LOG_INFO("电控 制动指令结束时间 ：" + currentTime3);
}

void DeviceManager::slot_actControl_new(int type)
{
    // ========== 核心逻辑：同物料不动作，保持原有姿态 ==========
    static bool firstFlag = true;
    if(!firstFlag)
    {
        if(type == m_lastMaterial)
        {
            //"相同物料，不执行任何转向，保持现状";
            return;
        }
    }
    firstFlag = false;
    m_lastMaterial = type;

    // ========== 物料发生切换，需要执行转向 ==========
    if(type == 3)//1号万向轮 左转
    {
        execW1SwitchToLeft();
    }
    else if(type == 2)//1号万向轮 右转
    {
        execW1SwitchToRight();
    }
    else if(type == 5)//2号万向轮 左转
    {
        execW1IDLE();
        QTimer::singleShot(500, this, [=]() {
            execW2SwitchToLeft();
        });
    }
    else if(type == 6)//2号万向轮 右转
    {
        execW1IDLE();
        QTimer::singleShot(500, this, [=]() {
            execW2SwitchToRight();
        });

    }
    else if(type == 4)//推杆
    {
        QTimer::singleShot(m_delayMsL2, this, [=]() {
            m_siemensModbusPlc->pushOnOff(2, true);

            QTimer::singleShot(1500, this, [=]() {
                m_siemensModbusPlc->pushOnOff(2, false);
            });
        });
    }
    else if(type == 7)//拨杆
    {
        QTimer::singleShot(m_delayMsL1, this, [=]() {
            m_siemensModbusPlc->pushOnOff(1, true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->pushOnOff(1, false);
            });
        });
    }
    else//未知
    {
        execW1IDLE();
        QTimer::singleShot(500, this, [=]() {
           execW2IDLE();
        });

    }
}

void DeviceManager::slot_larZhou_beltStop()
{
    //皮带静止
    LOG_INFO("接收来自拉曼PLC 皮带停止信号");
    // m_siemensModbusPlc->beltOnOff(6,false);
    // m_siemensModbusPlc->beltOnOff(5,false);
    // m_siemensModbusPlc->beltOnOff(4,false);
    m_siemensModbusPlc->stopReadLarZhou();
}

void DeviceManager::slot_larZhou_focusOn()
{
    LOG_INFO("接收来自拉曼PLC 聚焦完成信号");
    //结束循环读取线圈
    m_siemensModbusPlc->stopReadLarZhou();
    //larmanCapture();

}

void DeviceManager::beltOpen(int num, bool isopen)
{
    m_siemensModbusPlc->beltOnOff(num,isopen);
}

void DeviceManager::beltSpeed(int num, int speed)
{
    m_siemensModbusPlc->beltSpeedControl(num,speed);
}

void DeviceManager::pushControl(int num,bool op)
{
    m_siemensModbusPlc->pushOnOff(num,op);
}

void DeviceManager::turnControl(int num,int order)
{
    if(order == 0)
    {
        m_siemensModbusPlc->turnOnOff(num,false);
    }
    else if(order == 1)
    {
        m_siemensModbusPlc->turnOnOff(num,true);
    }
    else if(order == 2)
    {
        m_siemensModbusPlc->turnZuo(num,true);
    }
    else if(order == 3)
    {
        m_siemensModbusPlc->turnZuo(num,false);
    }
    else if(order == 4)
    {
        m_siemensModbusPlc->turnYou(num,true);
    }
    else if(order == 5)
    {
        m_siemensModbusPlc->turnYou(num,false);
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

void DeviceManager::isLarZhouStart(bool isok)
{
    if(isok)
    {
        m_siemensModbusPlc->setLarZhouStart();
    }
    else {
        m_siemensModbusPlc->setLarZhouStop();
    }
}


void DeviceManager::test()
{
    // int type = QRandomGenerator::global()->bounded(8);
    // emit sig_plasticType(type);

    std::string imgPath = "E:/test/555.jpeg";

    // 3. imread读取原图，IMREAD_COLOR读取彩色
    cv::Mat srcMat = cv::imread(imgPath, cv::IMREAD_COLOR);
    if (srcMat.empty())
    {
        qDebug() << "图像读取失败";
        return;
    }

    // 4. 彩色图转为灰度图，存入成员变量 m_grayMat
    cv::Mat m_grayMat,m_mergeMat,m_grayDrawMat;
    double X,Y;
    cv::cvtColor(srcMat, m_grayMat, cv::COLOR_BGR2GRAY);
    m_HikCamera->objectLocate(m_grayMat,m_mergeMat,X,Y,m_grayDrawMat);

    // 弹窗显示灰度图
    //cv::imshow("Draw Image", m_grayDrawMat);
    slot_onHikCaptureArrived(m_grayDrawMat);

    emit sig_hikObjectXY(X,Y); //物体定位
}

void DeviceManager::larmantest()
{
    // //拉曼检测

    // //告诉运动轴该物体需要检测
    // bool aaa = m_siemensModbusPlc->setLarZhouStart();
    // if(aaa)
    // {
    //     LOG_INFO("拉曼PLC 成功告诉运动轴该物体需要检测");
    // }
    m_siemensModbusPlc->startReadLarZhou();
    m_siemensModbusPlc->startReadLarZhou2();



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

void DeviceManager::lamanActControl(int type)
{
    larZhou_beltStartStop(true);
    //执行制动
    switch (type)
    {
    case 7:
        QTimer::singleShot(m_delayMsL1 - m_larmanDelay, this, [=]() {
            m_siemensModbusPlc->pushOnOff(1, true);

            QTimer::singleShot(1000, this, [=]() {
                m_siemensModbusPlc->pushOnOff(1, false);
            });
        });
        break;

    case 5:
        QTimer::singleShot(m_delayMsL2 - m_larmanDelay, this, [=]() {
            m_siemensModbusPlc->pushOnOff(2, true);

            QTimer::singleShot(1500, this, [=]() {
                m_siemensModbusPlc->pushOnOff(2, false);
            });
        });
        break;

    case 3:
        QTimer::singleShot(m_delayMsL3 - m_larmanDelay, this, [=]() {
            m_siemensModbusPlc->turnZuo(1,true);

            QTimer::singleShot(2000, this, [=]() {
                m_siemensModbusPlc->turnZuo(1,false);
            });
        });
        break;

    case 2:
        QTimer::singleShot(m_delayMsL3 - m_larmanDelay, this, [=]() {
            m_siemensModbusPlc->turnYou(1,true);

            QTimer::singleShot(2000, this, [=]() {
                m_siemensModbusPlc->turnYou(1,false);
            });
        });
        break;

    case 1:
        QTimer::singleShot(m_delayMsL4 - m_larmanDelay, this, [=]() {
            m_siemensModbusPlc->turnZuo(2,true);

            QTimer::singleShot(2000, this, [=]() {
                m_siemensModbusPlc->turnZuo(2,false);
            });
        });
        break;

    case 4:
        QTimer::singleShot(m_delayMsL4 - m_larmanDelay, this, [=]() {
            m_siemensModbusPlc->turnYou(2,true);

            QTimer::singleShot(2000, this, [=]() {
                m_siemensModbusPlc->turnYou(2,false);
            });
        });
        break;

    }
    QString currentTime3 = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("电控 制动指令结束时间 ：" + currentTime3);
}

void DeviceManager::larZhou_beltStartStop(bool ok)
{
    if(ok)
    {
        LOG_INFO("拉曼PLC 皮带启动");
    }
    else
    {
        LOG_INFO("拉曼PLC 皮带停止");
    }

    m_siemensModbusPlc->beltOnOff(6,ok);
}

void DeviceManager::execW1SwitchToLeft()
{
    // 硬件约束：如果当前是RIGHT状态，必须先右转回正
    if(m_curW1State == WheelRealState::RIGHT)
    {
        // 当前是右转，先执行右转回正";
        m_siemensModbusPlc->turnYou(1,false);
        m_curW1State = WheelRealState::IDLE;
        // 再执行左转
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            m_siemensModbusPlc->turnZuo(1,true);
        });

        m_curW1State = WheelRealState::LEFT;
    }

    // 当前已经IDLE，直接左转
    QTimer::singleShot(m_delayMsL3, this, [=]() {
        m_siemensModbusPlc->turnZuo(1,true);
    });
    m_curW1State = WheelRealState::LEFT;

    return;
}

void DeviceManager::execW1SwitchToRight()
{
    if(m_curW1State == WheelRealState::LEFT)
    {
        // 左转回正
        m_siemensModbusPlc->turnZuo(1,false);
        m_curW1State = WheelRealState::IDLE;
        // 再执行右转
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            m_siemensModbusPlc->turnYou(1,true);

        });
        m_curW1State = WheelRealState::RIGHT;
    }

    // 当前已经IDLE，直接右转
    QTimer::singleShot(m_delayMsL3, this, [=]() {
        m_siemensModbusPlc->turnYou(1,true);

    });
    m_curW1State = WheelRealState::RIGHT;

    return;
}

void DeviceManager::execW2SwitchToLeft()
{
    // 硬件约束：如果当前是RIGHT状态，必须先右转回正
    if(m_curW2State == WheelRealState::RIGHT)
    {
        // 当前是右转，先执行右转回正";
        m_siemensModbusPlc->turnYou(2,false);
        m_curW2State = WheelRealState::IDLE;
        // 再执行左转
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            m_siemensModbusPlc->turnZuo(2,true);

        });

        m_curW2State = WheelRealState::LEFT;
    }

    // 当前已经IDLE，直接左转
    QTimer::singleShot(m_delayMsL4, this, [=]() {
        m_siemensModbusPlc->turnZuo(2,true);

    });

    m_curW2State = WheelRealState::LEFT;

    return;
}

void DeviceManager::execW2SwitchToRight()
{
    if(m_curW2State == WheelRealState::LEFT)
    {
        // 左转回正
        m_siemensModbusPlc->turnZuo(2,false);
        m_curW2State = WheelRealState::IDLE;
        // 再执行右转
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            m_siemensModbusPlc->turnYou(2,true);

        });

        m_curW2State = WheelRealState::RIGHT;
    }

    // 当前已经IDLE，直接右转
    QTimer::singleShot(m_delayMsL4, this, [=]() {
        m_siemensModbusPlc->turnYou(2,true);

    });

    m_curW2State = WheelRealState::RIGHT;

    return;
}

void DeviceManager::execW1IDLE()
{
    if(m_curW1State == WheelRealState::LEFT)
    {
        // 左转回正
        m_siemensModbusPlc->turnZuo(1,false);
    }

    if(m_curW1State == WheelRealState::RIGHT)
    {
        // 左转回正
        m_siemensModbusPlc->turnYou(1,false);
    }

    m_curW1State = WheelRealState::IDLE;
}

void DeviceManager::execW2IDLE()
{

    if(m_curW2State == WheelRealState::LEFT)
    {
        // 左转回正
        m_siemensModbusPlc->turnZuo(2,false);
    }

    if(m_curW2State == WheelRealState::RIGHT)
    {
        // 左转回正
        m_siemensModbusPlc->turnYou(2,false);
    }

    m_curW2State = WheelRealState::IDLE;
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
    emit sig_plasticType(type);

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


    // //20260817 lcp add 拉曼检测
    // //m_siemensModbusPlc->startReadLarZhou2();
    // larZhou_beltStartStop(false);
    // m_siemensModbusPlc->setLarZhouStart();

    // // QTimer::singleShot(m_delayMsL0, this, [=]() {

    // //     larmanCapture();
    // // });

}
