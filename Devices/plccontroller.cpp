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
    plcdisconnect();
    stopReadReg();
    qDebug() << "PLC电控 析构释放";
}

void PlcController::init()
{
    if(m_modbus)
        return;

    m_modbus = new QModbusTcpClient(this);
    m_modbus->setTimeout(1000);
    m_modbus->setNumberOfRetries(3);

    m_readTimer = new QTimer(this);
    m_readTimer->setInterval(m_readInterval);

    // 定时读取槽函数
    connect(m_readTimer, &QTimer::timeout, this, &PlcController::readRegLoop);
}

bool PlcController::plcconnect(const QString &ip, quint16 port)
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
            return false;
        }
    }

    m_connected = true;

    return true;
}

void PlcController::plcdisconnect()
{
    if(m_modbus)
    {
        m_modbus->disconnectDevice();
    }

    m_connected = false;
}

bool PlcController::beltOnOff(int num ,bool startstop)
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

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

bool PlcController::beltSpeedControl(int num ,int frequency)
{
    if(!m_connected)
        return false;

    if(num < 1 || num > 9)
    {
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

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

bool PlcController::pushOnOff(int num, bool startstop)
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
    }
    else
    {
        unit.setValue(0, 0);
    }

    auto *reply = m_modbus->sendWriteRequest(unit, 1);
    if (!reply)
        return false;

    while (!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

bool PlcController::turnOnOff(int num ,bool isok)
{
    if(!m_connected){return false;}

    quint16 regAddr;
    if(num == 1)
    {
        regAddr = 315;
    }
    else
    {
        regAddr = 318;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,regAddr,1);

    if(isok)
    {
        unit.setValue(0,1);
    }
    else
    {
        unit.setValue(0,0);
    }

    auto *reply = m_modbus->sendWriteRequest(unit, 1);
    if(!reply){return false;}

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

bool PlcController::turnZuo(int num,bool iszuo)
{
    if(!m_connected){return false;}
    quint16 regAddr;
    if(num == 1)
    {
        regAddr = 313;
    }
    else
    {
        regAddr = 316;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,regAddr,1);

    if(iszuo)
    {
        unit.setValue(0,1);
    }
    else
    {
        unit.setValue(0,0);//回正
    }

    auto *reply = m_modbus->sendWriteRequest(unit, 1);

    if(!reply){return false;}

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

bool PlcController::turnYou(int num,bool isyou)
{
    if(!m_connected){return false;}
    quint16 regAddr;
    if(num == 1)
    {
        regAddr = 314;
    }
    else
    {
        regAddr = 317;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,regAddr,1);

    if(isyou)
    {
        unit.setValue(0,1);
    }
    else
    {
        unit.setValue(0,0);//回正
    }

    auto *reply = m_modbus->sendWriteRequest(unit, 1);

    if(!reply){return false;}

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

bool PlcController::setLarZhouStart()
{
    if(!m_connected){return false;}
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,m_writeLoopAdress_LarZhouStart,1);//HoldingRegisters  //DiscreteInputs
    unit.setValue(0,1);

    auto *reply = m_modbus->sendWriteRequest(unit, 1);
    if(!reply){return false;}

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

bool PlcController::setLarZhouStop()
{
    if(!m_connected){return false;}
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,m_writeLoopAdress_LarZhouStart,1);//HoldingRegisters  //DiscreteInputs
    unit.setValue(0,0);

    auto *reply = m_modbus->sendWriteRequest(unit, 1);
    if(!reply){return false;}

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    bool ok = (reply->error() == QModbusDevice::NoError);
    delete reply;
    return ok;
}

void PlcController::startReadReg()
{
    if(m_readTimer->isActive())
        return;
    // 先读取一次初始值，避免首次误判变化
    readRegLoop();
    m_readTimer->start();
}

void PlcController::stopReadReg()
{
    if(m_readTimer->isActive())
    {
        m_readTimer->stop();
    }
}

void PlcController::readRegLoop()
{
    // 1. PLC未连接直接返回
    if (!m_connected)
    {
        qDebug()<<"读取寄存器失败：PLC未连接";
        return;
    }

    // 构造读取请求：保持寄存器，地址330，读取1个寄存器
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, m_readLoopAdress, 1);//HoldingRegisters  DiscreteInputs
    QModbusReply* reply = m_modbus->sendReadRequest(readUnit, 1);
    if (!reply)
    {
        qDebug()<<"离散输入读取失败：发送请求为空";
        return;
    }

    // 等待通讯完成，增加超时保护500ms
    QElapsedTimer timer;
    timer.start();
    while (!reply->isFinished() && timer.elapsed() < 500)
    {
        QCoreApplication::processEvents();
    }

    bool readOk = (reply->error() == QModbusDevice::NoError);
    if (!readOk)
    {
        qDebug()<<QString("读取离散输入通讯错误：%1").arg(reply->errorString());
        delete reply;
        return;
    }

    // 获取当前寄存器数值
    quint16 currVal = reply->result().value(0);
    qDebug()<<"光栅当前离散值："<<currVal;
    emit sig_guangshanValue(currVal);

    delete reply;

    // 对比上次值，发生变化则触发信号+日志
    if ((currVal != m_lastRegVal)&(currVal == 1))
    {
        emit sig_regChanged();
    }
    m_lastRegVal = currVal;
}

void PlcController::readRegLarZhou()
{
    // 1. PLC未连接直接返回
    if (!m_connected)
    {
        qDebug()<<"读取寄存器失败：PLC未连接";
        return;
    }

    // 构造读取请求
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, m_readLoopAdress_LarZhou, 1);//HoldingRegisters  //DiscreteInputs
    QModbusReply* reply = m_modbus->sendReadRequest(readUnit, 1);
    if (!reply)
    {
        qDebug()<<"离散输入读取失败：发送请求为空";
        return;
    }

    // 等待通讯完成，增加超时保护500ms
    QElapsedTimer timer;
    timer.start();
    while (!reply->isFinished() && timer.elapsed() < 500)
    {
        QCoreApplication::processEvents();
    }

    bool readOk = (reply->error() == QModbusDevice::NoError);
    if (!readOk)
    {
        qDebug()<<QString("读取离散输入通讯错误：%1").arg(reply->errorString());
        delete reply;
        return;
    }

    // 获取当前寄存器数值
    quint16 currVal = reply->result().value(0);
    qDebug()<<"当前离散值："<<currVal;

    if (currVal == 1)
    {
        emit sig_regBeltStop();
    }

    delete reply;
}

void PlcController::readRegLarZhou2()
{
    // 1. PLC未连接直接返回
    if (!m_connected)
    {
        qDebug()<<"读取寄存器失败：PLC未连接";
        return;
    }

    // 构造读取请求
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, m_readLoopAdress_LarZhou2, 1);//HoldingRegisters  //DiscreteInputs
    QModbusReply* reply = m_modbus->sendReadRequest(readUnit, 1);
    if (!reply)
    {
        qDebug()<<"离散输入读取失败：发送请求为空";
        return;
    }

    // 等待通讯完成，增加超时保护500ms
    QElapsedTimer timer;
    timer.start();
    while (!reply->isFinished() && timer.elapsed() < 500)
    {
        QCoreApplication::processEvents();
    }

    bool readOk = (reply->error() == QModbusDevice::NoError);
    if (!readOk)
    {
        qDebug()<<QString("读取离散输入通讯错误：%1").arg(reply->errorString());
        delete reply;
        return;
    }

    // 获取当前寄存器数值
    quint16 currVal = reply->result().value(0);
    qDebug()<<"当前离散值："<<currVal;

    if (currVal == 1)
    {
        emit sig_regFocusON();
    }

    delete reply;
}
