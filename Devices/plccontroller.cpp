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

bool PlcController::turnControl(int open)
{
    if(!m_connected)
        return false;


    quint16 regAddr =
        313;

    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        regAddr,
        1);

    unit.setValue(
        0,
        open);

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
            QString("万向轮失败"));
    }

    delete reply;

    return ok;
}

bool PlcController::zuoControl()
{
    if(!m_connected)
        return false;


    quint16 regAddr =
        314;

    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        regAddr,
        1);

    unit.setValue(
        0,
        1);

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


    delete reply;

    return ok;
}

bool PlcController::youControl()
{
    if(!m_connected)
        return false;


    quint16 regAddr =
        315;

    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        regAddr,
        1);

    unit.setValue(
        0,
        1);

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


    delete reply;

    return ok;
}

bool PlcController::zuoControl2()
{
    if(!m_connected)
        return false;


    quint16 regAddr =
        314;

    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        regAddr,
        1);

    unit.setValue(
        0,
        0);

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


    delete reply;

    return ok;
}

bool PlcController::youControl2()
{
    if(!m_connected)
        return false;


    quint16 regAddr =
        315;

    QModbusDataUnit unit(
        QModbusDataUnit::HoldingRegisters,
        regAddr,
        1);

    unit.setValue(
        0,
        0);

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
    emit logInfo("开始循环读取寄存器330");
}

void PlcController::stopReadReg()
{
    if(m_readTimer->isActive())
    {
        m_readTimer->stop();
        emit logInfo("停止循环读取寄存器330");
    }
}

void PlcController::readRegLoop()
{
    // 1. PLC未连接直接返回
    if (!m_connected)
    {
        emit logInfo("读取寄存器失败：PLC未连接");
        return;
    }

    // 构造读取请求：保持寄存器，地址330，读取1个寄存器
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, m_readLoopAdress, 1);
    QModbusReply* reply = m_modbus->sendReadRequest(readUnit, 1);
    if (!reply)
    {
        emit logInfo("寄存器330读取失败：发送请求为空");
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
        emit logInfo(QString("读取330寄存器通讯错误：%1").arg(reply->errorString()));
        delete reply;
        return;
    }

    // 获取当前寄存器数值
    quint16 currVal = reply->result().value(0);
    delete reply;

    // 对比上次值，发生变化则触发信号+日志
    if (currVal != m_lastRegVal)
    {
        quint16 old = m_lastRegVal;
        m_lastRegVal = currVal;
        emit sig_regChanged(old, currVal);
    }
}

void PlcController::setReadLoopAdress(quint16 adr)
{
    m_readLoopAdress = adr;
}
