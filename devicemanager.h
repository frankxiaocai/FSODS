#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMessageBox>
#include <QtConcurrent>
#include "./Devices/hikcamera.h"
#include "./Devices/hyperspectralcamera.h"
#include "./Devices/larmanmodbustcp.h"
#include "./Devices/modbusworker.h"
#include "./CoreTools/fileio.h"
#include "./CoreTools/logger.h"
#include "./CoreTools/RamanPlasticRecognizer.h"
#include "./CoreTools/HSIPlasticRecognizer/HSIProcessor.h"

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    //初始化
    void init();
    Error_code initCamera();
    Error_code initLumo();
    Error_code initLarman();
    Error_code initEleControl();

    //设备采集
    void HIKCapture();
    Error_code lumoCapture(int XNum);
    Error_code larmanCapture();

    //参数设置
    void setdelayMsL0(int lt){m_delayMsL0 = lt;}
    void setdelayMsL1(int lt){m_delayMsL1 = lt;}
    void setdelayMsL2(int lt){m_delayMsL2 = lt;}
    void setdelayMsL3(int lt){m_delayMsL3 = lt;}
    void setdelayMsL4(int lt){m_delayMsL4 = lt;}
    void setlarmanDelay(int lt){m_larmanDelay = lt;}
    void setIsSave(bool aaa){m_isSave = aaa;}
    void setExposure(double aaa);
    void setFrameRate(double aaa);
    void setXLines(int line){m_XLines = line;}
    void setRunMode(bool isfastMode);

    //制动控制
    void beltOpen(int num,bool isopen);
    void beltSpeed(int num,int speed);
    void pushControl(int num,bool op);
    void turnControl(int num,int order);
    void beltOpenAll(bool isOpen);//一键启停

    //设置拉曼运动轴是否允许对焦
    void setLarZhouOI(bool isok);

    // 物体计数
    void updateObjectCount(int objType);
    int getObjTypeCount(int type);
    int getObjTotalCount();
    void clearAllObjectCount();

    void test();//测试函数

private:
    //设备+算法实例
    HikCamera* m_HikCamera = nullptr;
    HyperspectralCamera* m_HyperspectralCamera = nullptr;
    LarmanModbusTCP* m_larmanModbusTCP = nullptr;
    ModbusWorker* m_modbusWorker = nullptr;//20260828 add 新版modbus

    HSIProcessor m_HSIClassifier;//HSI塑料分类算法
    RamanPlasticRecognizer m_RamanPlasticRecognizer;//拉曼塑料分类算法

    int m_lastRegVal = 0;//上一次光栅值
    bool m_isFastMode{true};//快速模式

    //高光谱参数
    double m_Exposure = 10;//曝光时间 ms
    double m_FrameRate = 200;//帧率
    int m_XLines = 40;//采集行数
    bool m_isSave = false;//是否保存标识位

    //制动延迟时间
    int m_delayMsL0 = 0;//光栅-拉曼延迟 ms
    int m_delayMsL1 = 1000;//拨杆
    int m_delayMsL2 = 2000;//推杆
    int m_delayMsL3 = 3000;//1号万向轮
    int m_delayMsL4 = 4000;//2号万向轮
    int m_larmanDelay = 900;//拉曼单独控制逻辑延迟差

    //物体计数
    int m_objCount[9] = {0}; // 1~7种塑料 + 未知
    int m_objTotal = 0;//总数

    //-------------- modbus地址  --------------
    //写
    quint16 m_adress_belt1OI = 300;//1号皮带启停 启动：1 停止：0
    quint16 m_adress_belt2OI = 301;
    quint16 m_adress_belt3OI = 302;
    quint16 m_adress_belt4OI = 303;
    quint16 m_adress_belt5OI = 304;
    quint16 m_adress_belt6OI = 305;
    quint16 m_adress_belt7OI = 306;
    quint16 m_adress_belt8OI = 307;
    quint16 m_adress_belt9OI = 308;

    quint16 m_adress_belt1Speed = 100;//1号皮带速度
    quint16 m_adress_belt2Speed = 101;
    quint16 m_adress_belt3Speed = 102;
    quint16 m_adress_belt4Speed = 103;
    quint16 m_adress_belt5Speed = 104;
    quint16 m_adress_belt6Speed = 105;
    quint16 m_adress_belt7Speed = 106;
    quint16 m_adress_belt8Speed = 107;
    quint16 m_adress_belt9Speed = 108;

    quint16 m_adress_shiftOI = 309;//拨杆 启动：1 停止：0
    quint16 m_adress_pushOI = 311;//推杆 启动：1 停止：0
    quint16 m_adress_wheel1OI = 315;//1号万向轮启停 启动：1 停止：0
    quint16 m_adress_wheel2OI = 318;//2号万向轮启停 启动：1 停止：0
    quint16 m_adress_wheel1_left = 313;//1号万向轮左转+回正 左转：1 回正：0
    quint16 m_adress_wheel2_left = 316;//2号万向轮左转+回正 左转：1 回正：0
    quint16 m_adress_wheel1_right = 314;//1号万向轮右转+回正 右转：1 回正：0
    quint16 m_adress_wheel2_right = 317;//2号万向轮右转+回正 右转：1 回正：0
    quint16 m_adress_larZhouOI = 3600;//运动轴是否允许对焦 允许：1 不允许：0

    //读
    quint16 m_adress_grating = 600;// 高光谱光栅地址
    quint16 m_adress_LarZhou_belt = 3500;//运动轴让皮带停止地址
    quint16 m_adress_LarZhou_focusOn = 3501;//运动轴聚焦完成地址

private:
    void writeBatch2Raw(const HyperLineBatch &batch,int type);//保存采集光谱+类型数据
    QImage Mat2QImage(const cv::Mat &mat);
    QString RamanErrorCodeToChinese(RamanErrorCode code);//拉曼错误码

private slots:
    void slot_onObjectArrived();//光栅检测物体到了处理
    void slot_onFrameArrived(const HyperLineBatch &batch);//高光谱采集结果处理
    void slot_actControl(int type);//高光谱-制动
    void slot_lamanActControl(int type);//拉曼-制动
    void slot_pollReadDone(int regAddr,quint16 val);//轮询结果

signals:
    void sig_plasticType_hsi(int type);// 高光谱-塑料识别结果信号
    void sig_plasticType_larman(int type);// 拉曼-塑料识别结果信号
    void sig_guangshanValue(int aaa);//光栅值
};

#endif // DEVICEMANAGER_H
