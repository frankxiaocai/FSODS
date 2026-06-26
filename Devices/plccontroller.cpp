#include "plccontroller.h"

#include <QVariant>
#include <QModbusReply>
#include <QModbusDataUnit>
#include <QElapsedTimer>
#include <QCoreApplication>

PlcController::PlcController(QObject *parent)
    : QObject(parent)
{
}

PlcController::~PlcController()
{
    disconnect();

}

void PlcController::init()
{
    if(m_modbus)
        return;

    m_modbus = new QModbusTcpClient(this);

    m_modbus->setTimeout(1000);

    m_modbus->setNumberOfRetries(3);
}

bool PlcController::connect(const QString &ip, quint16 port)
{
    init();

    m_ip = ip;
    m_port = port;

    m_modbus->setConnectionParameter(
        QModbusDevice::NetworkAddressParameter,
        QVariant(ip));

    m_modbus->setConnectionParameter(
        QModbusDevice::NetworkPortParameter,
        QVariant(port));

    if(!m_modbus->connectDevice())
    {
        emit logInfo("连接失败");

        return false;
    }

    QElapsedTimer timer;

    timer.start();

    while(m_modbus->state()
           != QModbusDevice::ConnectedState)
    {
        QCoreApplication::processEvents();

        if(timer.elapsed() > 3000)
        {
            emit logInfo("连接超时");

            return false;
        }
    }

    m_connected = true;

    emit logInfo(
        QString("PLC连接成功: %1:%2")
            .arg(ip)
            .arg(port));

    return true;
}

void PlcController::disconnect()
{
    if(m_modbus)
    {
        m_modbus->disconnectDevice();
    }

    m_connected = false;
}

bool PlcController::isPlcconnect()
{
    return m_connected;
}

bool PlcController::beltControl(int num ,bool startstop)
{
    if(!m_connected)
        return false;

    if(num < 1 || num > 9)
    {

        return false;
    }

    quint16 regAddr =
        300 + (num - 1);

    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        regAddr,
        1);

    unit.setValue(
        0,
        startstop ? 1 : 0);

    auto *reply =
        m_modbus->sendWriteRequest(unit, 1);

    if(!reply)
        return false;

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok =
        (reply->error() == QModbusDevice::NoError);

    if(!ok)
    {
        emit logInfo(
            QString("变频器%1启停控制失败:%2")
                .arg(num)
                .arg(reply->errorString()));
    }

    delete reply;

    return ok;
}

bool PlcController::beltSpeedControl(int num ,int frequency)
{
    if(!m_connected)
        return false;

    if(num < 1 || num > 9)
    {
        emit logInfo(
            QString("变频器范围错误")
                .arg(num));
        return false;
    }

    quint16 regAddr =
        100 + (num - 1);

    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        regAddr,
        1);

    unit.setValue(
        0,
        static_cast<quint16>(frequency));

    auto *reply =
        m_modbus->sendWriteRequest(unit, 1);

    if(!reply)
        return false;

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok =
        (reply->error() == QModbusDevice::NoError);

    if(!ok)
    {
        emit logInfo(
            QString("变频器%1频率写入失败:%2")
                .arg(num)
                .arg(reply->errorString()));
    }

    delete reply;

    return ok;
}

bool PlcController::pushControltest(int num, bool startstop)
{
    if (!m_connected)
        return false;

    if (num < 1 || num > 2)
        return false;

    quint16 addrOpen  = 309 + (num - 1) * 2;
    quint16 addrClose = 310 + (num - 1) * 2;


    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        addrOpen,
        1);

    if (startstop)
    {
        unit.setValue(0,1);
        //unit.setValue(1, 0);
    }
    else
    {
        unit.setValue(0, 0);
        //unit.setValue(1, 1);
    }

    auto *reply = m_modbus->sendWriteRequest(unit, 1);
    if (!reply)
        return false;

    while (!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok = (reply->error() == QModbusDevice::NoError);

    if (!ok)
    {
        emit logInfo(
            QString("气动阀%1%2控制失败:%3")
                .arg(num)
                .arg(startstop ? "开" : "关")
                .arg(reply->errorString()));
    }

    delete reply;
    return ok;
}
