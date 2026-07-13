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
    void disconnect();
    bool isPlcconnect();

    bool beltControl(int num ,bool startstop);//皮带启停控制 num1-9
    bool beltSpeedControl(int num ,int Frequency);//皮带速度控制 num1-9 Frequency0-5000
    bool pushControltest(int num ,bool startstop);//ok
    bool turnControl(int open);
    bool zuoControl();
    bool youControl();
    bool zuoControl2();
    bool youControl2();

    //循环读取
    void startReadReg();
    void stopReadReg();
    void readRegLoop();

signals:

    void logInfo(const QString &logMes);
    // 寄存器发生变化信号，对外抛出新旧值
    void sig_regChanged();

private:
    QModbusTcpClient *m_modbus = nullptr;
    bool m_connected = false;
    QString m_ip;
    quint16 m_port = 502;

    // 读取定时器
    QTimer* m_readTimer;
    //循环读取地址
    quint16 m_readLoopAdress = 001;
    // 保存上一次寄存器的值，用于对比变化
    quint16 m_lastRegVal = 0;
    // 读取周期ms，可自行调整
    const int m_readInterval = 200;
};

#endif // PLCCONTROLLER_H
