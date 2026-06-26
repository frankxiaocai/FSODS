#ifndef LARMANMODBUSTCP_H
#define LARMANMODBUSTCP_H

#include <QObject>
#include <QHostAddress>
#include <QByteArray>
#include <QVector>
#include <cstdint>
#include <QThread>
#include <QElapsedTimer>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QVariant>
#include <QEventLoop>
#include <QTimer>
#include <QtAlgorithms>
#include <qminmax.h>
#include "../CoreTools/mystruct.h"

class LarmanModbusTCP : public QObject
{
    Q_OBJECT


public:
    explicit LarmanModbusTCP(QObject *parent = nullptr);
    ~LarmanModbusTCP();

    // ---------------------------设备连接---------------------------
    Error_code connectToDevice(const QString &ip, int port = MODBUS_TCP_PORT);
    void disconnectFromDevice();
    bool isConnected() const;

    // -------------------------- 设备自检 --------------------------
    Error_code checkDeviceSelfTest(SpectrometerStatus &status);
    Error_code checkLaserSelfTest(uint8_t laserCh, SpectrometerStatus &status);

    // -------------------------- 激光器控制 --------------------------
    Error_code setLaserChannel(uint8_t ch);
    Error_code setLaserWorkMode(LaserWorkMode mode);
    Error_code setLaserPower(uint16_t power_mW);
    Error_code setIntegralTime(uint32_t time);
    Error_code setAverageCount(uint16_t count);

    // -------------------------- 电机控制 --------------------------
    Error_code motorResetAndWait(MotorAxis axis);
    Error_code motorMoveRelativeAndWait(MotorAxis axis, int16_t distance_um);

    // -------------------------- 光谱采集 --------------------------
    Error_code startSpectrumCollect();
    Error_code getCollectStatus(SpectrometerStatus &collecting);
    Error_code getWavelengthData(QVector<float> &outWavelength);
    Error_code getOriginalSpectrum(QVector<float> &outSpectrum);

signals:
    void sig_limitStop(MotorAxis axis);

private:
    QModbusTcpClient *m_modbusClient;
    int m_modbustcptimeout;
    uint8_t m_slaveId;//光谱仪ID

private:
    // -------------------------- 基础寄存器操作 --------------------------
    // 【读】uint16保持寄存器
    Error_code readHoldingRegisters(quint16 regAddr, quint16 len, QVector<quint16> &outValues);
    // 【写】uint16保持寄存器
    Error_code writeHoldingRegisters(quint16 startAddr, const QVector<quint16>& values);
    Error_code writeHoldingRegisters(quint16 startAddr, quint16 value);//重载

    // -------------------------- 辅助 --------------------------
    Error_code readFloatRegs(uint16_t startReg, uint16_t floatCount, QVector<float> &outValues);
    float regsToFloat(uint16_t high, uint16_t low);
    QVector<uint16_t> uint32ToRegs(uint32_t val);

    // -------------------------- 电机底层 --------------------------
    Error_code motorReset(MotorAxis axis);
    Error_code motorStop(MotorAxis axis);
    Error_code motorMoveRelative(MotorAxis axis, int16_t distance_um);
    Error_code motorMoveAbsolute(MotorAxis axis, uint16_t stepDiv10);
    Error_code motorMoveAbsoluteAndWait(MotorAxis axis, uint16_t stepDiv10);
    Error_code getMotorPosition(MotorAxis axis, uint16_t &stepDiv10);
    Error_code getMotorStatus(MotorAxis axis, SpectrometerStatus &isMoving);

    // -------------------------- 校准 --------------------------
    Error_code saveCalibPosition();
    Error_code startCalibration();
    Error_code getCalibStatus(SpectrometerStatus &calibing);

    // -------------------------- 光谱仪参数 --------------------------
    Error_code getPixelCount(uint16_t &count);
    Error_code setPreprocessParam(const QVector<uint16_t> &params);//预处理参数
    Error_code getProcessedSpectrum(QVector<float> &outSpectrum);

private:
    static const uint16_t MODBUS_TCP_PORT = 502;             // 端口
    static const uint8_t  SPECTROMETER_SLAVE_ID = 100;       // 从站地址（一台网口设备内部，可以集成多个 Modbus 从站，用 SlaveID 区分）
    static const int      MODBUS_TIMEOUT_MS = 3000;          // modbus超时时间
    static const uint16_t MAX_PIXEL_COUNT = 1024;            // 像素数量
    static const uint16_t MAX_REG_PER_READ = 64;             // 单次最大读64个寄存器

    static const int XLimitStop = 25000;
    static const int ZLimitStop = 40000;
    static const int MOTOR_WAIT_TIMEOUT = 15000;

    //寄存器地址
    static const uint16_t REG_SET_INTEGRATION_TIME       = 0x1E;   // 设置积分时间 (2个寄存器)
    static const uint16_t REG_SET_PREPROCESS_PARAM       = 0x100;  // 设置预处理参数 (10个寄存器)
    static const uint16_t REG_SET_LASER_POWER            = 0x25;   // 设置激光器功率
    static const uint16_t REG_SET_LASER_WORK_MODE        = 0x26;   // 设置激光器工作模式
    static const uint16_t REG_SET_AVERAGE_COUNT          = 0x2F;   // 设置平均次数
    static const uint16_t REG_READ_PIXEL_COUNT           = 0x7E;   // 读取光谱仪支持的像素点
    static const uint16_t REG_SET_LASER_CHANNEL_INDEX    = 0x7C;   // 设置激光器通道index

    static const uint16_t REG_START_SPECTRUM_ACQUISITION = 0x62;   // 开始采集光谱指令
    static const uint16_t REG_READ_ACQUISITION_STATUS    = 0x70;   // 读取光谱仪采集是否完毕的状态
    static const uint16_t REG_READ_RAW_SPECTRUM_BASE     = 30000;  // 读取原始光谱数据起始地址
    static const uint16_t REG_READ_PRO_SPECTRUM_BASE     = 40000;  // 读取预处理光谱数据起始地址
    static const uint16_t REG_READ_WAVELENGTH_BASE       = 10000;  // 读取波长数组起始地址

    static const uint16_t REG_READ_SPECTROMETER_SELFCHECK = 0x80;  // 读取光谱仪自检状态
    static const uint16_t REG_READ_LASER1_SELFCHECK      = 0x81;   // 读取激光器1自检状态
    static const uint16_t REG_READ_LASER2_SELFCHECK      = 0x82;   // 读取激光器2自检状态
    static const uint16_t REG_READ_LASER3_SELFCHECK      = 0x83;   // 读取激光器3自检状态

    static const uint16_t REG_X_AXIS_MOVE_RELATIVE       = 0x84;   // X轴针对当前位置，移动相对距离
    static const uint16_t REG_X_AXIS_MOVE_ABSOLUTE       = 0x8C;   // X轴针移动到绝对位置
    static const uint16_t REG_X_AXIS_RESET               = 0x85;   // X轴归0
    static const uint16_t REG_X_AXIS_STOP                = 0x86;   // X移动停止
    static const uint16_t REG_READ_X_AXIS_POSITION       = 0x90;   // 读取X当前绝对位置
    static const uint16_t REG_READ_X_AXIS_STATUS         = 0x87;   // 读取X移动状态

    static const uint16_t REG_Z_AXIS_MOVE_RELATIVE       = 0x88;   // Z轴针对当前位置，移动相对距离
    static const uint16_t REG_Z_AXIS_MOVE_ABSOLUTE       = 0x8D;   // Z轴针移动到绝对位置
    static const uint16_t REG_Z_AXIS_RESET               = 0x89;   // Z轴归0
    static const uint16_t REG_Z_AXIS_STOP                = 0x8A;   // Z移动停止
    static const uint16_t REG_READ_Z_AXIS_POSITION       = 0x91;   // 读取Z当前绝对位置
    static const uint16_t REG_READ_Z_AXIS_STATUS         = 0x8B;   // 读取Z移动状态

    static const uint16_t REG_SAVE_CALIBRATION_POSITION  = 0x9B;   // 保存当前位置为校准位置
    static const uint16_t REG_EXECUTE_CALIBRATION_ACTION = 0x9C;   // 执行校准动作
    static const uint16_t REG_READ_CALIBRATION_STATUS    = 0x9E;   // 读取校准动作状态寄存器
};

#endif // LARMANMODBUSTCP_H
