#ifndef MODBUSWORKER_H
#define MODBUSWORKER_H

#include <QObject>
#include <QModbusTcpClient>
#include <QTimer>
#include <QQueue>
#include <QModbusDataUnit>
#include <QVector>

class ModbusWorker : public QObject
{
    Q_OBJECT
public:
    explicit ModbusWorker(QObject *parent = nullptr);
    ~ModbusWorker();

    void init();
    // 连接PLC
    void plcconnect(const QString& ip, quint16 port = 502);
    void plcdisconnect();

    // 启动/停止普通轮询
    void startPoll(int pollIntervalMs = 200);
    void stopPoll();

signals:
    // 提交【紧急写】任务信号
    void sigUrgentWrite(quint16 addr, quint16 value, const QString& tag);

    // 日志信号
    void sig_logMsg(const QString& msg);
    // 轮询【读】完成信号
    void sig_pollReadDone(quint16 regAddr,quint16 val);
    // 【紧急写】完成信号
    void sig_urgentWriteFinished(bool ok, const QString& tag, quint64 reqSubmitMs, quint64 reqDoneMs);

private slots:
    // 提交【紧急写】任务槽函数
    void urgentWriteHoldingReg(quint16 addr, quint16 value, const QString& tag = "");

    // 普通轮询槽：周期发送【读】请求
    void onPollTimerTimeout();

    // 【写】完成回调
    void onWriteFinished(QModbusReply* reply);
    // 【读】完成回调
    void onReadFinished(QModbusReply* reply);

private:
    QModbusTcpClient* m_modbusClient{nullptr};
    QTimer* m_pollTimer{nullptr};

    bool m_isUrgentWriting{false};
    bool m_pollBusy{false};//轮询标识位，防止并发报文风暴

    QVector<int> m_pollRegAddrs = {600,3500,3501};//轮询地址
    int m_pollIndex{0};//当前读到第几个寄存器
};

#endif // MODBUSWORKER_H
