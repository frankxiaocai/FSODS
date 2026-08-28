#include "modbusworker.h"
#include <QDebug>
#include <QModbusReply>
#include <QDateTime>

ModbusWorker::ModbusWorker(QObject *parent)
    : QObject(parent)
{
    m_modbusClient = new QModbusTcpClient(this);
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);

    connect(m_pollTimer, &QTimer::timeout,this, &ModbusWorker::onPollTimerTimeout);
    //信号绑定到槽，强制QueuedConnection，任务投递到worker子线程
    connect(this, &ModbusWorker::sigUrgentWrite,this, &ModbusWorker::urgentWriteHoldingReg, Qt::QueuedConnection);
}

ModbusWorker::~ModbusWorker()
{
    stopPoll();
    if(m_modbusClient->state() != QModbusDevice::UnconnectedState)
    {
        m_modbusClient->disconnectDevice();
    }
}


void ModbusWorker::plcconnect(const QString &ip, quint16 port)
{
    m_modbusClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_modbusClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    m_modbusClient->connectDevice();
    emit sig_logMsg(QString("尝试连接PLC %1:%2").arg(ip).arg(port));
}

void ModbusWorker::plcdisconnect()
{
    stopPoll();
    m_urgentQueue.clear();
    m_isUrgentWriting = false;
    m_pollBusy = false;
    m_modbusClient->disconnectDevice();
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
void ModbusWorker::urgentWriteHoldingReg(int addr, quint16 value, const QString &tag)
{
    if(m_modbusClient->state() != QModbusDevice::ConnectedState)
    {
        emit sig_logMsg("Modbus未连接，丢弃紧急写");
        return;
    }

    UrgentWriteItem item;
    item.regAddr = addr;
    item.value = value;
    item.tag = tag;//任务名
    item.submitMs = QDateTime::currentMSecsSinceEpoch();

    m_urgentQueue.enqueue(item);//任务入队
    emit sig_logMsg(QString("[%1]收到紧急写任务 tag:%2 addr:%3 val:%4")
                    .arg(item.submitMs).arg(tag).arg(addr).arg(value));

    // 如果当前没有正在跑紧急写，立刻处理下一个
    if(!m_isUrgentWriting)
    {
        processNextUrgentWrite();
    }
}

// 执行下一个紧急写任务
void ModbusWorker::processNextUrgentWrite()
{
    if(m_urgentQueue.isEmpty())
    {
        m_isUrgentWriting = false;
        return;
    }
    m_isUrgentWriting = true;

    UrgentWriteItem item = m_urgentQueue.dequeue();

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters,item.regAddr,QVector<quint16>{item.value});

    QModbusReply* reply = m_modbusClient->sendWriteRequest(writeUnit, 1);
    if(!reply)
    {
        emit sig_logMsg(QString("紧急写发送失败 %1").arg(m_modbusClient->errorString()));
        m_isUrgentWriting = false;
        QMetaObject::invokeMethod(this, &ModbusWorker::processNextUrgentWrite, Qt::QueuedConnection);
        return;
    }

    reply->setProperty("urgentTag", item.tag);
    reply->setProperty("urgentSubmitMs", item.submitMs);

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

    // 处理队列中下一个紧急任务
    m_isUrgentWriting = false;
    processNextUrgentWrite();
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
            emit sig_logMsg(QString("轮询读取寄存器[%1] = %2").arg(pollAddr).arg(val));
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
