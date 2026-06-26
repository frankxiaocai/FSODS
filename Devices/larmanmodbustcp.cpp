#include "larmanmodbustcp.h"
#include <cstring>
#include <QDebug>

LarmanModbusTCP::LarmanModbusTCP(QObject *parent)
    : QObject(parent),
    m_modbusClient(new QModbusTcpClient(this)),
    m_modbustcptimeout(MODBUS_TIMEOUT_MS),
    m_slaveId(SPECTROMETER_SLAVE_ID)
{
    m_modbusClient->setTimeout(m_modbustcptimeout);
    m_modbusClient->setNumberOfRetries(2);
}

LarmanModbusTCP::~LarmanModbusTCP()
{
    disconnectFromDevice();
    qDebug() << "LarmanModbusTCP 析构释放";

}

Error_code LarmanModbusTCP::connectToDevice(const QString &ip, int port)
{
    if (isConnected())
    {
        m_modbusClient->disconnectDevice();
    }

    // 设置ModbusTCP参数
    m_modbusClient->setConnectionParameter(QModbusTcpClient::NetworkAddressParameter, ip);
    m_modbusClient->setConnectionParameter(QModbusTcpClient::NetworkPortParameter, port);

    if (!m_modbusClient->connectDevice())
    {
        qDebug() << "连接启动失败：" << m_modbusClient->errorString();
        return Error_ConnectFailed;
    }

    // ===================== 等待连接成功/失败  =====================
    QEventLoop loop;
    QTimer connectTimer;
    connectTimer.setSingleShot(true);
    connectTimer.setInterval(m_modbustcptimeout); // 连接超时3秒

    // 超时退出
    connect(&connectTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    // 状态改变退出
    connect(m_modbusClient, &QModbusDevice::stateChanged, &loop, [&](QModbusDevice::State state) {
        if (state == QModbusDevice::ConnectedState || state == QModbusDevice::UnconnectedState) {
            loop.quit();
        }
    });

    connectTimer.start();
    loop.exec();

    // ===================== 最终判断 =====================
    if (m_modbusClient->state() == QModbusDevice::ConnectedState)
    {
        qDebug() << "Modbus TCP 连接成功：" << ip << port;
        return Error_None;
    }
    else
    {
        qDebug() << "Modbus TCP 连接失败！当前状态：" << m_modbusClient->state();
        return Error_ConnectFailed;
    }
}

void LarmanModbusTCP::disconnectFromDevice()
{
    if (isConnected())
    {
        m_modbusClient->disconnectDevice();
    }
}

bool LarmanModbusTCP::isConnected() const
{
    return m_modbusClient->state() == QModbusDevice::ConnectedState;
}

Error_code LarmanModbusTCP::readHoldingRegisters(quint16 regAddr, quint16 len, QVector<quint16> &outValues)
{
    // 1. 构造读取数据单元
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, regAddr, len);

    // 2. 发送读请求
    QModbusReply *reply = m_modbusClient->sendReadRequest(readUnit, m_slaveId);
    if (!reply)
    {
        qDebug() << "读请求发送失败：" << m_modbusClient->errorString()
            << " 地址：" << regAddr << " 长度：" << len
            << " SlaveID：" << m_slaveId
            << " 连接状态：" << m_modbusClient->state();
        return Error_RequestFailed;
    }

    // 3. 绑定父对象，防止对象销毁时野指针/内存泄漏
    reply->setParent(this);

    // 4. 同步等待响应（标准同步写法）
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);

    timeoutTimer.start(m_modbustcptimeout);
    loop.exec();

    // 5. 超时判断
    if (!timeoutTimer.isActive())
    {
        qDebug() << "读寄存器超时：地址=" << regAddr;
        reply->deleteLater();
        return Error_ReadRegFailed;
    }

    // 6. 正常响应，停止定时器
    timeoutTimer.stop();

    // 7. 处理返回结果
    Error_code result = Error_ReadRegFailed;
    if (reply->error() == QModbusDevice::NoError)
    {
        // 读取数据
        outValues.clear();
        const QModbusDataUnit unit = reply->result();
        for (int i = 0; i < unit.valueCount(); ++i) {
            outValues.append(unit.value(i));
        }

        qDebug() << QString("读寄存器成功：地址=%1 长度=%2").arg(regAddr).arg(len);
        result = Error_None;
    }
    else
    {
        qDebug() << "读寄存器失败：" << reply->errorString() << " 地址：" << regAddr;
    }

    // 8. 统一释放 reply（任何分支最终都会释放）
    reply->deleteLater();

    return result;
}


Error_code LarmanModbusTCP::writeHoldingRegisters(quint16 startAddr, const QVector<quint16> &values)
{
    // 1. 空数据检查
    if (values.isEmpty())
    {
        qDebug() << "写入失败：数据为空";
        return Error_WriteRegFailed;
    }

    // 2. 构造写入单元
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, startAddr, values.size());
    for (int i = 0; i < values.size(); ++i) {
        unit.setValue(i, values[i]);
    }

    // 3. 发送请求
    QModbusReply *reply = m_modbusClient->sendWriteRequest(unit, m_slaveId);
    if (!reply)
    {
        qDebug() << "写入请求发送失败：" << m_modbusClient->errorString();
        return Error_WriteRegFailed;
    }

    // 4. 父对象管理，防止内存泄漏 & 野指针
    reply->setParent(this);

    // 5. 同步等待（标准同步写法）
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    // 超时/完成 都退出事件循环
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);

    timeoutTimer.start(m_modbustcptimeout);
    loop.exec();

    // 6. 超时判断
    if (!timeoutTimer.isActive())
    {
        qDebug() << "写寄存器超时：地址=" << startAddr;
        reply->deleteLater();
        return Error_WriteRegFailed;
    }

    // 7. 停止定时器
    timeoutTimer.stop();

    // 8. 错误判断
    Error_code result = Error_WriteRegFailed;
    if (reply->error() == QModbusDevice::NoError)
    {
        qDebug() << QString("写入成功：地址=%1 数量=%2")
                        .arg(startAddr).arg(values.size());
        result = Error_None;
    }
    else
    {
        qDebug() << "写入失败：" << reply->errorString();
    }

    // 9. 统一释放 reply（最规范写法）
    reply->deleteLater();

    return result;
}

Error_code LarmanModbusTCP::writeHoldingRegisters(quint16 startAddr, quint16 value)
{
    // 1. 构造单个寄存器写入单元
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, startAddr, 1);
    unit.setValue(0, value);

    // 2. 发送写入请求
    QModbusReply *reply = m_modbusClient->sendWriteRequest(unit, m_slaveId);
    if (!reply)
    {
        // 增加错误日志，方便排查发送失败原因
        qDebug() << "写入请求发送失败：" << m_modbusClient->errorString();
        return Error_WriteRegFailed;
    }

    // 3. 绑定父对象，确保异常退出/析构时自动释放，防止野指针 & 内存泄漏
    reply->setParent(this);

    // 4. 同步等待响应（标准 Qt 同步写法）
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);

    timeoutTimer.start(m_modbustcptimeout);
    loop.exec();

    // 5. 超时判断
    if (!timeoutTimer.isActive())
    {
        qDebug() << "写寄存器超时：地址=" << startAddr;
        reply->deleteLater();
        return Error_WriteRegFailed;
    }

    // 6. 正常响应，停止定时器
    timeoutTimer.stop();

    // 7. 处理返回结果
    Error_code result = Error_WriteRegFailed;
    if (reply->error() == QModbusDevice::NoError)
    {
        qDebug() << QString("写入成功：地址=%1 值=%2").arg(startAddr).arg(value);
        result = Error_None;
    }
    else
    {
        qDebug() << QString("写入失败：地址=%1，错误：%2")
                        .arg(startAddr).arg(reply->errorString());
    }

    // 8. 统一释放 reply（最安全，任何路径都能释放）
    reply->deleteLater();

    return result;
}
/**
 * @brief LarmanModbusTCP::readFloatRegs 从设备读取多个浮点数(1个float = 2个uint16寄存器)
 * @param startReg 起始寄存器地址
 * @param floatCount 要读取的浮点数数量
 * @param outValues 读取到的 float 数组
 * @return
 */
Error_code LarmanModbusTCP::readFloatRegs(uint16_t startReg, uint16_t floatCount, QVector<float> &outValues)
{
    outValues.clear();

    // ====================== 1. 参数合法性校验 ======================
    if (floatCount == 0) {return Error_InvalidData;}

    // ====================== 2. 定义合理常量 ======================
    const uint16_t totalRegCount = floatCount * 2;//  1float = 2寄存器
    QVector<uint16_t> allRegs;
    allRegs.reserve(totalRegCount);  // 预分配内存

    // ====================== 3. 分批读取寄存器 ======================
    for (uint16_t offset = 0; offset < totalRegCount; offset += MAX_REG_PER_READ)
    {
        const uint16_t readCnt = qMin<uint16_t>(MAX_REG_PER_READ, totalRegCount - offset);
        const uint16_t currStartReg = startReg + offset;
        QVector<uint16_t> regs;

        Error_code err = readHoldingRegisters(currStartReg, readCnt, regs);
        qDebug()<<currStartReg;

        if (err != Error_None) {
            qDebug() << "分批读取寄存器失败，地址:" << currStartReg << "错误码:" << err;
            return err;
        }

        allRegs.append(regs);
    }

    // ====================== 4. 转换为浮点数 ======================
    // 两两组合转 float
    for (int i = 0; i < allRegs.size(); i += 2)
    {
        float value = regsToFloat(allRegs[i+1], allRegs[i]);
        outValues.append(value);
    }

    qDebug() << "读取浮点数完成，数量:" << outValues.size();
    return Error_None;
}

// ===================== 设备自检 =====================
Error_code LarmanModbusTCP::checkDeviceSelfTest(SpectrometerStatus &status)
{
    QVector<uint16_t> regs;
    Error_code err = readHoldingRegisters(REG_READ_SPECTROMETER_SELFCHECK, 1, regs);
    if (err) return err;
    status = (SpectrometerStatus)regs[0];
    return Error_None;
}

Error_code LarmanModbusTCP::checkLaserSelfTest(uint8_t laserCh, SpectrometerStatus &status)
{
    if (laserCh > 2) return Error_InvalidData;
    QVector<uint16_t> regs;
    Error_code err = readHoldingRegisters(REG_READ_LASER1_SELFCHECK + laserCh, 1, regs);
    if (err) return err;
    status = (SpectrometerStatus)regs[0];
    return Error_None;
}

// ===================== 激光器控制 =====================
Error_code LarmanModbusTCP::setLaserChannel(uint8_t ch)
{
    if (ch > 2) return Error_InvalidData;
    return writeHoldingRegisters(REG_SET_LASER_CHANNEL_INDEX, ch);
}

Error_code LarmanModbusTCP::setLaserWorkMode(LaserWorkMode mode)
{
    return writeHoldingRegisters(REG_SET_LASER_WORK_MODE, (uint16_t)mode);
}

Error_code LarmanModbusTCP::setLaserPower(uint16_t power_mW)
{
    return writeHoldingRegisters(REG_SET_LASER_POWER, power_mW);
}

Error_code LarmanModbusTCP::setIntegralTime(uint32_t time)
{
    return writeHoldingRegisters(REG_SET_INTEGRATION_TIME, uint32ToRegs(time));
}

Error_code LarmanModbusTCP::setAverageCount(uint16_t count)
{
    return writeHoldingRegisters(REG_SET_AVERAGE_COUNT, count);
}

// ===================== 电机 =====================
Error_code LarmanModbusTCP::motorReset(MotorAxis axis)
{
    return writeHoldingRegisters(axis == Axis_X ? REG_X_AXIS_RESET : REG_Z_AXIS_RESET, 0);
}

Error_code LarmanModbusTCP::motorStop(MotorAxis axis)
{
    return writeHoldingRegisters(axis == Axis_X ? REG_X_AXIS_STOP : REG_Z_AXIS_STOP, 0);
}

Error_code LarmanModbusTCP::motorMoveRelative(MotorAxis axis, int16_t distance_um)
{
    return writeHoldingRegisters(axis == Axis_X ? REG_X_AXIS_MOVE_RELATIVE : REG_Z_AXIS_MOVE_RELATIVE, distance_um);
}

Error_code LarmanModbusTCP::motorMoveAbsolute(MotorAxis axis, uint16_t stepDiv10)
{
    return writeHoldingRegisters(axis == Axis_X ? REG_X_AXIS_MOVE_ABSOLUTE : REG_Z_AXIS_MOVE_ABSOLUTE, stepDiv10);
}

Error_code LarmanModbusTCP::getMotorPosition(MotorAxis axis, uint16_t &stepDiv10)
{
    QVector<uint16_t> regs;
    Error_code err = readHoldingRegisters(axis == Axis_X ? REG_READ_X_AXIS_POSITION : REG_READ_Z_AXIS_POSITION, 1, regs);
    if (err) return err;
    stepDiv10 = regs[0];
    return Error_None;
}
/**
 * @brief LarmanModbusTCP::getMotorStatus
 * @param axis
 * @param isMoving 0:移动完毕   1：移动中
 * @return
 */
Error_code LarmanModbusTCP::getMotorStatus(MotorAxis axis, SpectrometerStatus &isMoving)
{
    QVector<uint16_t> regs;
    Error_code err = readHoldingRegisters(axis == Axis_X ? REG_READ_X_AXIS_STATUS : REG_READ_Z_AXIS_STATUS, 1, regs);
    if (err) return err;
    isMoving = (SpectrometerStatus)regs[0];
    return Error_None;
}

/**
 * @brief LarmanModbusTCP::motorResetAndWait 电机复位
 * @param axis
 * @return
 */
Error_code LarmanModbusTCP::motorResetAndWait(MotorAxis axis)
{
    // 1. 发送复位指令
    Error_code err = motorReset(axis);
    if (err != Error_None) {
        qDebug() << "电机复位发送失败，轴：" << axis << "错误码：" << err;
        return err;
    }

    // 2. 启动超时计时器
    QElapsedTimer timer;
    timer.start();

    // 3. 循环等待复位完成
    while (true)
    {
        // 获取状态
        SpectrometerStatus status;
        err = getMotorStatus(axis, status);
        if (err != Error_None) {
            qDebug() << "获取电机状态失败，轴：" << axis << "错误码：" << err;
            return err;
        }

        // 复位成功：状态 == 0
        if (status == 0) {
            qDebug() << "电机复位完成，轴：" << axis;
            break;
        }

        // 超时判断
        if (timer.elapsed() > MOTOR_WAIT_TIMEOUT) {
            qDebug() << "电机复位超时，轴：" << axis << "超时时间：" << MOTOR_WAIT_TIMEOUT;
            return Error_XZMoveTimeout;
        }

        // 防止循环空跑导致 CPU 100%
        QThread::msleep(20);
    }

    return Error_None;
}


/**
 * @brief LarmanModbusTCP::motorMoveRelativeAndWait 电机相对位移
 * @param axis
 * @param distance_um
 * @return
 */
Error_code LarmanModbusTCP::motorMoveRelativeAndWait(MotorAxis axis, int16_t distance_um)
{
    // 1. 发送相对移动指令
    Error_code err = motorMoveRelative(axis, distance_um);
    if (err != Error_None)
    {
        qDebug() << QString("轴%1 启动相对移动失败：%2").arg(axis).arg(err);
        return err;
    }

    // 2. 启动超时计时器
    QElapsedTimer timer;
    timer.start();

    SpectrometerStatus status;

    // 3. 循环等待运动完成
    while (true)
    {
        // 获取电机状态
        err = getMotorStatus(axis, status);
        if (err != Error_None)
        {
            qDebug() << QString("轴%1 获取状态失败：%2").arg(axis).arg(err);
            return err;
        }

        // 状态 = 0 → 电机已停止
        if (status == 0)
        {
            // 获取当前位置
            uint16_t currentPos;
            err = getMotorPosition(axis, currentPos);
            if (err != Error_None)
            {
                qDebug() << QString("轴%1 获取位置失败：%2").arg(axis).arg(err);
                return err;
            }

            // 判断限位
            uint16_t limitPos = (axis == Axis_X) ? XLimitStop : ZLimitStop;
            if (currentPos == limitPos)
            {
                qDebug() << QString("轴%1 触发限位停止，位置=%2").arg(axis).arg(currentPos);
                emit sig_limitStop(axis);
                return Error_None; // 限位正常，不报错
            }

            // 正常停止
            qDebug() << QString("轴%1 移动完成，位置=%2").arg(axis).arg(currentPos);
            break;
        }

        // 超时判断
        if (timer.elapsed() > MOTOR_WAIT_TIMEOUT)
        {
            qDebug() << QString("轴%1 移动超时！").arg(axis);
            return Error_XZMoveTimeout;
        }

        // 加小延时，防止 CPU 100%
        QThread::msleep(20);
    }

    return Error_None;
}


/**
 * @brief LarmanModbusTCP::motorMoveAbsoluteAndWait 电机绝对位移
 * @param axis
 * @param stepDiv10
 * @return
 */
Error_code LarmanModbusTCP::motorMoveAbsoluteAndWait(MotorAxis axis, uint16_t stepDiv10)
{
    // 1. 发送绝对运动指令
    Error_code err = motorMoveAbsolute(axis, stepDiv10);
    if (err != Error_None) {
        qDebug() << QString("轴%1 启动绝对运动失败：%2").arg(axis).arg(err);
        return err;
    }

    // 2. 启动超时计时
    QElapsedTimer timer;
    timer.start();
    SpectrometerStatus status;

    // 3. 循环等待运动停止
    while (true) {
        // 获取电机状态
        err = getMotorStatus(axis, status);
        if (err != Error_None) {
            qDebug() << QString("轴%1 获取状态失败：%2").arg(axis).arg(err);
            return err;
        }

        // 电机已停止
        if (status == 0) {
            // 获取当前位置
            uint16_t currentPos;
            err = getMotorPosition(axis, currentPos);
            if (err != Error_None) {
                qDebug() << QString("轴%1 获取位置失败：%2").arg(axis).arg(err);
                return err;
            }

            // 限位判断
            uint16_t limitPos = (axis == Axis_X) ? XLimitStop : ZLimitStop;
            if (currentPos == limitPos) {
                qDebug() << QString("轴%1 触发限位停止 位置=%2").arg(axis).arg(currentPos);
                emit sig_limitStop(axis);
                return Error_None;
            }

            // 正常停止
            qDebug() << QString("轴%1 绝对运动完成 位置=%2").arg(axis).arg(currentPos);
            break;
        }

        // 超时判断
        if (timer.elapsed() > MOTOR_WAIT_TIMEOUT) {
            qDebug() << QString("轴%1 绝对运动超时！").arg(axis);
            return Error_XZMoveTimeout;
        }

        // 防止CPU 100%
        QThread::msleep(20);
    }

    return Error_None;
}

// ===================== 光谱采集 =====================
Error_code LarmanModbusTCP::startSpectrumCollect()
{
    return writeHoldingRegisters(REG_START_SPECTRUM_ACQUISITION, 1);
}

Error_code LarmanModbusTCP::getCollectStatus(SpectrometerStatus &collecting)
{
    QVector<uint16_t> regs;
    Error_code err = readHoldingRegisters(REG_READ_ACQUISITION_STATUS, 1, regs);
    if (err) return err;
    collecting = (SpectrometerStatus)regs[0];
    return Error_None;
}

Error_code LarmanModbusTCP::getWavelengthData(QVector<float> &outWavelength)
{
    return readFloatRegs(REG_READ_WAVELENGTH_BASE, MAX_PIXEL_COUNT, outWavelength);
}

Error_code LarmanModbusTCP::getOriginalSpectrum(QVector<float> &outSpectrum)
{
    return readFloatRegs(REG_READ_RAW_SPECTRUM_BASE, MAX_PIXEL_COUNT, outSpectrum);
}

Error_code LarmanModbusTCP::getProcessedSpectrum(QVector<float> &outSpectrum)
{
    return readFloatRegs(REG_READ_PRO_SPECTRUM_BASE, MAX_PIXEL_COUNT, outSpectrum);
}

// ===================== 校准 =====================
Error_code LarmanModbusTCP::saveCalibPosition()
{
    return writeHoldingRegisters(REG_SAVE_CALIBRATION_POSITION, 1);
}

Error_code LarmanModbusTCP::startCalibration()
{
    return writeHoldingRegisters(REG_EXECUTE_CALIBRATION_ACTION, 1);
}

Error_code LarmanModbusTCP::getCalibStatus(SpectrometerStatus &calibing)
{
    QVector<uint16_t> regs;
    Error_code err = readHoldingRegisters(REG_READ_CALIBRATION_STATUS, 1, regs);
    if (err) return err;
    calibing = (SpectrometerStatus)regs[0];
    return Error_None;
}

// ===================== 辅助 =====================
/**
 * @brief LarmanModbusTCP::regsToFloat 把 **两个16位寄存器** 拼接成 **一个32位float浮点数**
 * @param high 高16位
 * @param low 低16位
 * @return 解析后的 float 小数
 */
float LarmanModbusTCP::regsToFloat(uint16_t high, uint16_t low)
{
    // 1. 拼接成 32位无符号整数（高位在前：高16位左移16位 + 低16位）
    uint32_t uint32_val = ((uint32_t)high << 16) | low;

    // 2. 按二进制内存拷贝方式转成 float（不做数值运算，直接二进制解析）
    float result;
    memcpy(&result, &uint32_val, sizeof(float));

    return result;
}
/**
 * @brief LarmanModbusTCP::uint32ToRegs 把一个 32 位无符号整数，拆成 2 个 Modbus 16 位寄存器
 * @param val
 * @return 高16位寄存器、低16位寄存器
 */
QVector<uint16_t> LarmanModbusTCP::uint32ToRegs(uint32_t val)
{
    return { (uint16_t)(val >> 16), (uint16_t)val };
}

Error_code LarmanModbusTCP::getPixelCount(uint16_t &count)
{
    QVector<uint16_t> regs;
    Error_code err = readHoldingRegisters(REG_READ_PIXEL_COUNT, 1, regs);
    if (err) return err;
    count = regs[0];
    return Error_None;
}

Error_code LarmanModbusTCP::setPreprocessParam(const QVector<uint16_t> &params)
{
    if (params.size() != 10) return Error_InvalidData;
    return writeHoldingRegisters(REG_SET_PREPROCESS_PARAM, params);
}
