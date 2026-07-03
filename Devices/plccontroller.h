#ifndef PLCCONTROLLER_H
#define PLCCONTROLLER_H

#include <QObject>
#include <QModbusTcpClient>
#include <QModbusDataUnit>

class PlcController : public QObject
{
    Q_OBJECT

public:

    explicit PlcController(QObject *parent = nullptr);
    ~PlcController();

    void init();

    bool connect(const QString &ip,quint16 port = 502);
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

signals:

    void logInfo(const QString &logMes);

private:

    QModbusTcpClient *m_modbus = nullptr;

    bool m_connected = false;

    QString m_ip;

    quint16 m_port = 502;
};

#endif // PLCCONTROLLER_H
