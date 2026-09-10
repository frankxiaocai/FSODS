#include "devicemanager.h"

DeviceManager::DeviceManager(QObject *parent)
    : QObject{parent}
    ,m_HikCamera(new HikCamera(this))
    ,m_HyperspectralCamera(new HyperspectralCamera(this))
    ,m_larmanModbusTCP(new LarmanModbusTCP(this))
    ,m_modbusWorker(new ModbusWorker(this))
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
    // connect(m_HikCamera, &HikCamera::sig_newImage, this, &DeviceManager::sig_newImage);
    // connect(m_HikCamera, &HikCamera::sig_objectCapture, this, &DeviceManager::slot_onHikCaptureArrived);
    // connect(m_HikCamera, &HikCamera::sig_objectLocation, this, &DeviceManager::sig_hikObjectXY);
    // connect(m_HikCamera, &HikCamera::sig_objectLocation, this, &DeviceManager::slot_hikObjectXY);
    // 高光谱采集信号
    // connect(m_HyperspectralCamera, &HyperspectralCamera::sig_batchFinished,this,&DeviceManager::sig_batchFinished);
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

    m_modbusWorker->plcconnect("192.168.0.140",501);
    m_modbusWorker->startPoll(200);
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
        QString larmanerror = RamanErrorCodeToChinese(error);
        LOG_ERROR("拉曼塑料检测算法失败："+larmanerror);
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
    m_HyperspectralCamera->setExposure(m_Exposure);//曝光时间 ms
}

void DeviceManager::setFrameRate(double aaa)
{
    m_FrameRate = aaa;
    m_HyperspectralCamera->setFrameRate(m_FrameRate);//帧率
}

void DeviceManager::setRunMode(bool isfastMode)
{
    m_isFastMode = isfastMode;
    setLarZhouOI(!isfastMode);
    QString logStr = QString("当前模式： %1")
                         .arg(m_isFastMode ? "快检模式" : "精检模式");
    LOG_INFO(logStr);
}

void DeviceManager::slot_actControl(int type)
{
    switch (type)
    {
    case 1://拨杆
        QTimer::singleShot(m_delayMsL1, this, [=]() {
            pushControl(1, true);

            QTimer::singleShot(1000, this, [=]() {
                pushControl(1, false);
            });
        });
        break;

    case 4://推杆
        QTimer::singleShot(m_delayMsL2, this, [=]() {
            pushControl(2, true);

            QTimer::singleShot(1500, this, [=]() {
                pushControl(2, false);
            });
        });
        break;

    case 3://万向轮1 左
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            turnControl(1,2);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(1,3);
            });
        });
        break;

    case 2://万向轮1 右
        QTimer::singleShot(m_delayMsL3, this, [=]() {
            turnControl(1,4);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(1,5);
            });
        });
        break;

    case 5://万向轮2 左
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            turnControl(2,2);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(2,3);
            });
        });
        break;

    case 6://万向轮2 右
        QTimer::singleShot(m_delayMsL4, this, [=]() {
            turnControl(2,4);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(2,5);
            });
        });
        break;
    }
}

void DeviceManager::slot_pollReadDone(int regAddr, quint16 val)
{
    if(regAddr == m_adress_grating)//光栅
    {
        emit sig_guangshanValue(val);
        if ((val != m_lastRegVal)&(val == 1))
        {
            slot_onObjectArrived();
        }
        m_lastRegVal = val;
    }
    else
    {

    }
}

void DeviceManager::beltOpen(int num, bool isopen)
{
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
        emit m_modbusWorker->sigUrgentWrite(addr01, 0, QString("万向轮%1停止").arg(num));
    }
    else if(order == 1)//万向轮开
    {
        emit m_modbusWorker->sigUrgentWrite(addr01, 1, QString("万向轮%1启动").arg(num));
    }
    else if(order == 2)//万向轮左转
    {
        emit m_modbusWorker->sigUrgentWrite(addr23, 1, QString("万向轮%1左转").arg(num));
    }
    else if(order == 3)//万向轮左转回正
    {
        emit m_modbusWorker->sigUrgentWrite(addr23, 0, QString("万向轮%1左转回正").arg(num));
    }
    else if(order == 4)//万向轮右转
    {
        emit m_modbusWorker->sigUrgentWrite(addr45, 1, QString("万向轮%1右转").arg(num));
    }
    else if(order == 5)//万向轮右转回正
    {
        emit m_modbusWorker->sigUrgentWrite(addr45, 0, QString("万向轮%1右转回正").arg(num));
    }
}

void DeviceManager::beltOpenAll(bool isOpen)
{
    for(int i = 1; i < 10; ++i)
    {

        QTimer::singleShot(200*i-200, this, [=]() {
            beltOpen(i, isOpen);
        });
    }


    int order = isOpen? 1:0;
    QTimer::singleShot(1800, this, [=]() {
        turnControl(1,order);
    });

    QTimer::singleShot(2000, this, [=]() {
        turnControl(2,order);
    });


    int slow_speed5 = 6;
    int slow_speed6 = 15;
    if(m_isFastMode)
    {
        QTimer::singleShot(2200, this, [=]() {
            beltSpeed(5,50*200);
        });

        QTimer::singleShot(2400, this, [=]() {
            beltSpeed(6,50*200);
        });
    }
    else
    {
        QTimer::singleShot(2200, this, [=]() {
            beltSpeed(5,slow_speed5*200);
        });

        QTimer::singleShot(2400, this, [=]() {
            beltSpeed(6,slow_speed6*200);
        });
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
        emit m_modbusWorker->sigUrgentWrite(m_adress_larZhouOI, 1, "允许对焦");
    }
    else {
        emit m_modbusWorker->sigUrgentWrite(m_adress_larZhouOI, 0, "不允许对焦");
    }
}

void DeviceManager::test()
{

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

QString DeviceManager::RamanErrorCodeToChinese(RamanErrorCode code)
{
    switch (code)
    {
    case Error_None_raman:					return "无错误";
    case Error_InputSpectrumEmpty:			return "输入光谱为空";
    case Error_InputSpectrumSizeMismatch:	return "光谱数据长度不匹配";
    case Error_InputSpectrumTooFewPoints:	return "光谱有效点数过少";
    case Error_InputSpectrumInvalidValue:	return "光谱包含非法数值";
    case Error_InputSpectrumRangeTooSmall:	return "光谱横坐标范围过小";
    case Error_InputSpectrumDuplicateXTooMany: return "光谱横坐标重复点过多";

    case Error_TrainDirectoryEmpty:			return "训练文件夹为空";
    case Error_TrainDirectoryNotExist:		return "训练文件夹不存在";
    case Error_TrainDirectoryNotAccessible:	return "训练文件夹无访问权限";
    case Error_TrainCsvMissing:				return "训练CSV文件缺失";
    case Error_TrainCsvOpenFailed:			return "CSV文件打开失败";
    case Error_TrainCsvOccupied:			return "CSV文件被占用";
    case Error_TrainCsvReadFailed:			return "CSV读取失败";
    case Error_TrainCsvEmpty:				return "CSV文件内容为空";
    case Error_TrainCsvNoValidNumber:		return "CSV无有效数值";
    case Error_TrainCsvFormatInvalid:		return "CSV格式非法";
    case Error_TrainCsvDimensionInvalid:	return "CSV维度异常";
    case Error_TrainCsvContainsInvalidValue: return "CSV包含非法数值";
    case Error_TrainSampleEmpty:			return "训练样本为空";
    case Error_TrainSampleDimensionMismatch: return "训练样本维度不一致";

    case Error_BaselineCorrectionFailed:	return "基线校正失败";
    case Error_LinearSystemSolveFailed:		return "线性方程组求解失败";
    case Error_NormalizationFailed:			return "归一化处理失败";
    case Error_NormalizationZeroRange:		return "归一化区间为0，无法归一化";
    case Error_InterpolationFailed:			return "插值运算失败";
    case Error_FeatureExtractionFailed:	return "特征提取失败";
    case Error_FeatureDimensionInvalid:		return "输出特征维度非法";

    case Error_KnnKInvalid:					return "KNN的K值非法";
    case Error_KnnTrainSamplesEmpty:		return "KNN训练样本为空";
    case Error_KnnDistanceFailed:			return "KNN距离计算失败";
    case Error_KnnPredictionFailed:			return "KNN预测失败";
    case Error_PredictedLabelInvalid:		return "预测标签无效";

    case Error_MemoryAllocationFailed:		return "内存分配失败";
    case Error_StdException:					return "标准异常";
    case Error_UnknownException:			return "未知异常";

    default:
        return QString("未知错误码(%1)").arg(static_cast<int>(code));
    }
}

void DeviceManager::slot_lamanActControl(int type)
{
    switch (type)
    {
    case 1://拨杆
        QTimer::singleShot(m_delayMsL1 + m_larmanDelay, this, [=]() {
            pushControl(1, true);

            QTimer::singleShot(1000, this, [=]() {
                pushControl(1, false);
            });
        });
        break;

    case 4://推杆
        QTimer::singleShot(m_delayMsL2 + m_larmanDelay, this, [=]() {
            pushControl(2, true);

            QTimer::singleShot(1500, this, [=]() {
                pushControl(2, false);
            });
        });
        break;

    case 3://万向轮1 左
        QTimer::singleShot(m_delayMsL3 + m_larmanDelay, this, [=]() {
            turnControl(1,2);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(1,3);
            });
        });
        break;

    case 2://万向轮1 右
        QTimer::singleShot(m_delayMsL3 + m_larmanDelay, this, [=]() {
            turnControl(1,4);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(1,5);
            });
        });
        break;

    case 5://万向轮2 左
        QTimer::singleShot(m_delayMsL4 + m_larmanDelay, this, [=]() {
            turnControl(2,2);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(2,3);
            });
        });
        break;

    case 6://万向轮2 右
        QTimer::singleShot(m_delayMsL4 + m_larmanDelay, this, [=]() {
            turnControl(2,4);

            QTimer::singleShot(1000, this, [=]() {
                turnControl(2,5);
            });
        });
        break;

    }
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


void DeviceManager::slot_onObjectArrived()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_INFO("光栅 识别到物体" + currentTime);
    if(m_isFastMode)
    {
        lumoCapture(m_XLines);
    }
    else
    {
        // QTimer::singleShot(m_delayMsL0, this, [=]() {
        //     larmanCapture();
        // });
    }

}
