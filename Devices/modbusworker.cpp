#include "modbusworker.h"
#include <QDebug>
#include <QModbusReply>
#include <QDateTime>
#include <QElapsedTimer>
#include <QCoreApplication>

ModbusWorker::ModbusWorker(QObject *parent)
    : QObject(parent)
{

}

ModbusWorker::~ModbusWorker()
{
    stopPoll();
    if(m_modbusClient->state() != QModbusDevice::UnconnectedState)
    {
        m_modbusClient->disconnectDevice();
    }
    qDebug() << "ModbusWorker 析构释放";
}

void ModbusWorker::init()
{
    if(m_modbusClient)
        return;
    m_modbusClient = new QModbusTcpClient(this);
    m_modbusClient->setTimeout(1000);
    m_modbusClient->setNumberOfRetries(3);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);

    connect(m_pollTimer, &QTimer::timeout,this, &ModbusWorker::onPollTimerTimeout);
    connect(this, &ModbusWorker::sigUrgentWrite,this, &ModbusWorker::urgentWriteHoldingReg);//, Qt::QueuedConnection
}


void ModbusWorker::plcconnect(const QString &ip, quint16 port)
{
    init();
    m_modbusClient->setConnectionParameter(
        QModbusDevice::NetworkAddressParameter,
        QVariant(ip));

    m_modbusClient->setConnectionParameter(
        QModbusDevice::NetworkPortParameter,
        QVariant(port));

    if(!m_modbusClient->connectDevice())
    {
        return ;
    }

    QElapsedTimer timer;

    timer.start();

    while(m_modbusClient->state()
           != QModbusDevice::ConnectedState)
    {
        QCoreApplication::processEvents();

        if(timer.elapsed() > 3000)
        {
            return ;
        }
    }

    emit sig_logMsg(QString("成功连接PLC %1:%2").arg(ip).arg(port));
}

void ModbusWorker::plcdisconnect()
{
    stopPoll();
    m_isUrgentWriting = false;
    m_pollBusy = false;
    if(m_modbusClient)
    {
        m_modbusClient->disconnectDevice();
    }
}

void ModbusWorker::startPoll(int pollIntervalMs)
{
    m_pollTimer->setInterval(pollIntervalMs);
    m_pollTimer->start();
    emit sig_logMsg(QString("启动普通轮询，周期%1ms").arg(pollIntervalMs));
}

void ModbusWorker::stopPoll()
{
    m_pollTimer->stop();
    emit sig_logMsg("停止普通轮询");
}

// 提交紧急写任务
void ModbusWorker::urgentWriteHoldingReg(quint16 addr, quint16 value, const QString &tag)
{
    if(m_modbusClient->state() != QModbusDevice::ConnectedState)
    {
        emit sig_logMsg("Modbus未连接，丢弃紧急写");
        return;
    }

    m_isUrgentWriting = true;
    //-----------------------------------------------------------------------------
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters,addr,1);
    writeUnit.setValue(0,value);

    auto* reply = m_modbusClient->sendWriteRequest(writeUnit, 1);

    if(!reply)
    {
        emit sig_logMsg(QString("紧急写发送失败 %1").arg(m_modbusClient->errorString()));
        m_isUrgentWriting = false;
        return;
    }

    while(!reply->isFinished())
    {
        QCoreApplication::processEvents();
    }

    // 绑定reply完成信号
    connect(reply, &QModbusReply::finished, this, [this, reply](){
        this->onWriteFinished(reply);
    });

}

// 普通轮询：读取多个不连续保持寄存器
void ModbusWorker::onPollTimerTimeout()
{
    // 关键：正在紧急写，跳过本次轮询，实现插队
    if(m_isUrgentWriting)
    {
        return;
    }
    if(m_modbusClient->state() != QModbusDevice::ConnectedState)
    {
        return;
    }
    // 上一轮读还没返回，直接跳过本次定时器，避免并发请求
    if(m_pollBusy)
    {
        return;
    }

    m_pollIndex = 0;
    m_pollBusy = true;

    int regAddr = m_pollRegAddrs.at(m_pollIndex);
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, regAddr, 1);
    QModbusReply* reply = m_modbusClient->sendReadRequest(readUnit, 1);
    if(reply)
    {
        reply->setProperty("pollAddr", regAddr);
        connect(reply, &QModbusReply::finished, this, [this, reply](){
            this->onReadFinished(reply);
        });
    }
    else
    {
        m_pollBusy = false;
        emit sig_logMsg(QString("轮询读地址%1发送失败：%2").arg(regAddr).arg(m_modbusClient->errorString()));
    }
}

// 写完成回调
void ModbusWorker::onWriteFinished(QModbusReply *reply)
{
    if(!reply) return;

    quint64 doneMs = QDateTime::currentMSecsSinceEpoch();
    QString tag = reply->property("urgentTag").toString();
    quint64 submitMs = reply->property("urgentSubmitMs").toULongLong();

    bool ok = (reply->error() == QModbusDevice::NoError);
    emit sig_urgentWriteFinished(ok, tag, submitMs, doneMs);

    if(ok)
    {
        emit sig_logMsg(QString("[%1]紧急写完成 tag:%2 提交:%3 完成:%4 往返耗时%5ms")
                        .arg(doneMs).arg(tag).arg(submitMs).arg(doneMs).arg(doneMs-submitMs));
    }
    else
    {
        emit sig_logMsg(QString("紧急写错误:%1").arg(reply->errorString()));
    }

    reply->deleteLater();

    m_isUrgentWriting = false;
}

// 轮询【读】 完成回调
void ModbusWorker::onReadFinished(QModbusReply *reply)
{
    if(!reply) return;

    int pollAddr = reply->property("pollAddr").toInt();
    if(pollAddr > 0)
    {
        // 属于轮询任务
        if(reply->error() == QModbusDevice::NoError)
        {
            const QModbusDataUnit& unit = reply->result();
            quint16 val = unit.value(0);
            //emit sig_logMsg(QString("轮询读取寄存器[%1] = %2").arg(pollAddr).arg(val));
            emit sig_pollReadDone(pollAddr,val);
        }
        else
        {
            emit sig_logMsg(QString("轮询读寄存器[%1]失败：%2").arg(pollAddr).arg(reply->errorString()));
        }

        // 读取下一个不连续寄存器
        m_pollIndex++;
        if(m_pollIndex < m_pollRegAddrs.size())
        {
            int nextAddr = m_pollRegAddrs.at(m_pollIndex);
            QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, nextAddr, 1);
            QModbusReply* nextReply = m_modbusClient->sendReadRequest(readUnit,1);
            if(nextReply)
            {
                nextReply->setProperty("pollAddr", nextAddr);
                connect(nextReply, &QModbusReply::finished, this, [this, nextReply](){
                    this->onReadFinished(nextReply);
                });
            }
            else
            {
                m_pollBusy = false;
                emit sig_logMsg(QString("轮询读下一个地址%1失败").arg(nextAddr));
            }
        }
        else
        {
            // 全部寄存器读取完毕，释放busy标记
            m_pollBusy = false;
        }
    }

    reply->deleteLater();
}
