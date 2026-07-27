#ifndef MYSTRUCT_H
#define MYSTRUCT_H

#include <QHostAddress>
#include <QString>

// 单通道扫描参数结构体
struct scanConfigs
{
    // 时间参数 s
    int wtime = 30;//吹扫时间
    int ctime = 30;//取样时间
    // 位移参数 cm
    float disment_X = 25;
    float disement_Z = 30;
    // 零移参数 cm
    float zero_disment_X = 16.0f;
    float zero_disment_Z = 38.7f;
    // 面扫参数
    int areascan_numpoint = 2;
    float areascan_disment = 0.5f;//cm
    // 激光参数
    int laser_power = 60;//功率 毫瓦
    int laser_time = 10000;//积分时间 us
    int laser_frequency = 10;//积分频次

};
// 其他参数结构体
struct otherConfigs
{
    int m_timeout = 10;//超时时间 (min)
    QHostAddress Server1IP = QHostAddress("127.0.0.1");
    QHostAddress Server2IP = QHostAddress("127.0.0.1");
    int Server1Port = 502;
    int Server2Port = 503;
    int spacetime = 10;//间隔检测时间(min)
    QString burner1 = "燃烧器1";
    QString burner2 = "燃烧器2";
    QString burner3 = "燃烧器3";
    QString path1 = "D:/SSP";//高光谱配置文件目录
    QString path2 = "D:/SSP";
    QString path3 = "D:/SSP";

};

struct diankongConfigs
{
    int delayMsL0 = 0;//光栅-高光谱延迟
    int delayMsL1 = 2700;//1号制动延迟 ms
    int delayMsL2 = 5500;
    int delayMsL3 = 6500;
    int delayMsL4 = 7500;
    int delayMsLarman = 1000;
    double Exposure = 10;//曝光时间 ms
    double FrameRate = 200;//帧率
    int XLines = 40;//采集行数
};

// 错误码
enum Error_code : int
{
    Error_None = 0,
    Error_ConnectFailed,           // TCP连接失败
    Error_RequestFailed,           // 发送Modbus请求失败
    Error_ReadRegFailed,           // 读寄存器错误
    Error_WriteRegFailed,          // 写寄存器错误
    Error_DeviceTestselfFailed,    // 光谱仪+激光自检异常
    Error_InvalidData,             // 写入参数错误
    Error_XZMoveTimeout,           // 轴归零等待超时
    Error_ValveControlFailed,      // 气体阀门通断电失败
    Error_SingleDetectionTimeout,  // 光谱仪单次检测超时（倒计时）
    Error_DrawingImageFailed,      // 绘图参数错误
    Error_SaveCSVFailed,           // 保存CSV文件参数错误
    Error_InvalidParameter,        // 光谱解析失败
    Error_EleControl,              // 电控失败
    Error_Camera,
    Error_Hyperspectral,
    Error_Larman
};

enum  detectionMode
{
    SinglePoint,  // 单点模式
    Continuous    // 连续模式
};

// 气体阀门通道
enum Channel {
    Channel1 = 0,  // 1#探头
    Channel2 = 1,  // 2#探头
    Channel3 = 2   // 3#探头
};

// 阀门编号
enum ValveNo {
    Valve_A = 0,
    Valve_B = 1,
    Valve_C = 2
};
//Modbus
enum SpectrometerStatus : uint8_t
{
    Status_Error = 0,
    Status_Normal = 1
};

enum LaserWorkMode : uint16_t
{
    Laser_Close = 0,
    Laser_AlwaysOn = 1,
    Laser_Auto = 2
};

enum MotorAxis : uint8_t
{
    Axis_X,
    Axis_Z
};

#endif // MYSTRUCT_H
