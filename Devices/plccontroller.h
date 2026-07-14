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

    //循环读取光栅
    void startReadReg();
    void stopReadReg();
    void readRegLoop();

signals:
    void sig_regChanged();//光栅

private:
    QModbusTcpClient *m_modbus = nullptr;
    bool m_connected = false;
    QString m_ip;
    quint16 m_port = 502;

    // 循环读取定时器
    QTimer* m_readTimer;
    quint16 m_readLoopAdress = 001;
    quint16 m_lastRegVal = 0;
    const int m_readInterval = 200;//循环读取周期
};

#endif // PLCCONTROLLER_H
