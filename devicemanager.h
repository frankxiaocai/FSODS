#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMessageBox>
#include <QtConcurrent>
#include <QThread>
#include "./Devices/hikcamera.h"
#include "./Devices/hyperspectralcamera.h"
#include "./Devices/larmanmodbustcp.h"
#include "./Devices/plccontroller.h"
#include "./Devices/modbusworker.h"
#include "./CoreTools/fileio.h"
#include "./CoreTools/logger.h"
#include "./CoreTools/RamanPlasticRecognizer.h"
#include "./CoreTools/HSIPlasticRecognizer/HSIProcessor.h"
// 万向轮物理实际状态
enum class WheelRealState
{
    IDLE,       // 回正中位
    LEFT,       // 左转到位
    RIGHT       // 右转到位
};
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
    void setdelayMs_afterW1(int lt){m_delayMs_afterW1 = lt;}
    void setdelayMs_afterW2(int lt){m_delayMs_afterW2 = lt;}
    void setIsSave(bool aaa){m_isSave = aaa;}
    void setExposure(double aaa);
    void setFrameRate(double aaa);
    void setXLines(int line){m_XLines = line;}

    //制动控制
    void beltOpen(int num,bool isopen);
    void beltSpeed(int num,int speed);
    void pushControl(int num,bool op);//推杆 （序号）
    void turnControl(int num,int order);

    //设置拉曼运动轴是否允许对焦
    void setLarZhouOI(bool isok);

    // 物体计数
    void updateObjectCount(int objType);
    int getObjTypeCount(int type);
    int getObjTotalCount();
    void clearAllObjectCount();

    void test();//测试

private:
    //设备+算法实例
    HikCamera* m_HikCamera = nullptr;
    HyperspectralCamera* m_HyperspectralCamera = nullptr;
    LarmanModbusTCP* m_larmanModbusTCP = nullptr;
    PlcController* m_siemensModbusPlc = nullptr;
    ModbusWorker* m_modbusWorker = nullptr;//20260828 add 新版modbus
    QThread* m_workerThread = nullptr;//20260828 add 新版modbus

    HSIProcessor m_HSIClassifier;//HSI塑料分类算法
    RamanPlasticRecognizer m_RamanPlasticRecognizer;//拉曼塑料分类算法

    int m_lastRegVal = 0;//上一次光栅值

    //高光谱参数
    double m_Exposure = 10;//曝光时间 ms
    double m_FrameRate = 200;//帧率
    int m_XLines = 40;//采集行数
    bool m_isSave = false;//是否保存标识位

    //制动延迟时间
    int m_delayMsL0 = 0;//光栅-高光谱延迟 ms
    int m_delayMsL1 = 1000;//拨杆
    int m_delayMsL2 = 2000;//推杆
    int m_delayMsL3 = 3000;//1号万向轮
    int m_delayMsL4 = 4000;//2号万向轮
    int m_larmanDelay = 900;//拉曼单独控制逻辑延迟差
    int m_delayMs_afterW1 = 3000;//过1号万向轮
    int m_delayMs_afterW2 = 4000;//过2号万向轮

    //物料类型---制动方位
    int shift_type = 3;//拨杆
    int push_type = 2;//推杆
    int wheel1_left_type = 7;//1号轮 左转
    int wheel1_right_type = 4;//1号轮 右转
    int wheel2_left_type = 5;//2号轮 左转
    int wheel2_right_type = 6;//2号轮 右转

    //物体计数
    int m_objCount[9] = {0}; // 1~7种塑料 + 未知
    int m_objTotal = 0;//总数

    // 执行逻辑优化----20260826 add
    WheelRealState m_curW1State{WheelRealState::IDLE};//1号万向轮 当前状态
    WheelRealState m_curW2State{WheelRealState::IDLE};//2号万向轮 当前状态
    int m_lastMaterial = 999;//上一次物料类型

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

    //万向轮动作----20260831 add
    void wheelAct(int type);
    //万向轮回正
    void wheelReset(int type);
    void execW1IDLE();
    void execW2IDLE();
    //获取T1（检测线---动作执行时间）
    int getT1(int type);
    //获取T2（动作执行---结束时间）
    int getT2(int type);
    //万向轮控制逻辑
    void wheelActControl(int type);

private slots:
    void slot_onObjectArrived();//光栅检测物体到了处理
    void slot_onFrameArrived(const HyperLineBatch &batch);//高光谱采集结果处理
    void slot_onHikCaptureArrived(cv::Mat targetOnly);//相机定位图像处理
    void slot_hikObjectXY(double X,double Y); //相机定位位置处理
    void slot_actControl(int type);//高光谱-筛选控制(方案1)
    void slot_actControl_new2(int type);//高光谱-筛选控制逻辑优化----20260831 add(方案2)
    void slot_lamanActControl(int type);//拉曼-制动
    void slot_larZhou_beltStop();
    void slot_larZhou_focusOn();

    //modbus优化----20260828 add
    void slot_pollReadDone(int regAddr,quint16 val);//轮询结果

signals:
    void sig_newImage(const QImage& img);//相机图像流
    void sig_hikCaptured(const QImage& img); //相机定位图像
    void sig_hikObjectXY(double X,double Y); //相机定位位置
    void sig_batchFinished(const HyperLineBatch &batch);//高光谱采集结果
    void sig_plasticType_hsi(int type);// 高光谱-塑料识别结果信号
    void sig_plasticType_larman(int type);// 拉曼-塑料识别结果信号
    void sig_guangshanValue(int aaa);//光栅值
};

#endif // DEVICEMANAGER_H
