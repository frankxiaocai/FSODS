#ifndef PLCCONTROLLER_H
#define PLCCONTROLLER_H

#include <QObject>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QTimer>

class PlcController : public QObject
{
    Q_OBJECT

public:

    explicit PlcController(QObject *parent = nullptr);
    ~PlcController();

    void init();

    bool plcconnect(const QString &ip,quint16 port = 502);
    void plcdisconnect();

    bool beltOnOff(int num,bool startstop);//皮带启停
    bool beltSpeedControl(int num,int Frequency);//皮带速度
    bool pushOnOff(int num,bool startstop);//推杆拨杆
    bool turnOnOff(int num,bool isok);//万向轮开关
    bool turnZuo(int num,bool iszuo);//万向轮左转+回正
    bool turnYou(int num,bool isyou);//万向轮右转+回正

    bool setLarZhouStart();//物体需要拉曼检测
    bool setLarZhouStop();//物体停止拉曼检测

    //循环读取光栅
    void startReadReg();
    void stopReadReg();
    void readRegLoop();

    //循环读取拉曼运动轴
    void readRegLarZhou();
    void readRegLarZhou2();

signals:
    void sig_regChanged();//光栅
    void sig_guangshanValue(int guang);
    void sig_regBeltStop();//运动轴让皮带停止
    void sig_regFocusON();//运动轴聚焦完成

private:
    QModbusTcpClient *m_modbus = nullptr;
    bool m_connected = false;
    QString m_ip;
    quint16 m_port = 502;

    // 循环读取光栅定时器
    QTimer* m_readTimer;
    quint16 m_readLoopAdress = 600;
    quint16 m_lastRegVal = 0;
    const int m_readInterval = 200;//循环读取周期

    // 循环读取拉曼运动轴定时器
    quint16 m_readLoopAdress_LarZhou = 3500;//运动轴让皮带停止地址
    quint16 m_readLoopAdress_LarZhou2 = 3501;//运动轴聚焦完成地址
    quint16 m_writeLoopAdress_LarZhouStart = 3600;//告诉运动轴需要检测
};

#endif // PLCCONTROLLER_H
