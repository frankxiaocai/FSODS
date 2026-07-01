
#ifndef HYPERSPECTRALCAMERA_H
#define HYPERSPECTRALCAMERA_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QByteArray>
#include <cstring>
#include <vector>
#include "../CoreTools/mystruct.h"
#include "../CoreTools/HSIPlasticRecognizer/HSIProcessor.h"
#include "SI_sensor.h"
#include "SI_types.h"
#include "SI_errors.h"

// struct HyperLineBatch
// {
//     //QByteArray data;         // 按 BIL 拼接好的 X 行数据
//     std::vector<unsigned char> data;
//     int width = 0;           // 行宽（像素）
//     int bands = 0;          // 波段数
//     int bytesPerPixel = 2;   // 固定 2 字节
//     int requestedLines = 0;  //  预设 X
//     int receivedLines = 0;   // 实际收到
// };

class HyperspectralCamera : public QObject
{
    Q_OBJECT
public:
    explicit HyperspectralCamera(QObject *parent = nullptr);
    ~HyperspectralCamera();

    // --------------------------- 官方 9.1 设备枚举 ---------------------------
    int setSSP();
    bool loadSDK();
    bool unloadSDK();
    int deviceCount();
    QString deviceName(int index);

    // --------------------------- 官方 9.2 打开/初始化 ---------------------------
    bool openDevice(int index = 0);
    bool closeDevice();
    bool initialize();

    // --------------------------- 参数设置 ---------------------------
    bool setExposure(double ms);//曝光时间
    bool setFrameRate(double fps);//帧率
    bool setSpatialBin(int bin);//空间像素合并
    bool setSpectralBin(int bin);//光谱波段合并

    // --------------------------- 高光谱采集启停 ---------------------------
    bool startAcquisition();             // 开启采集
    bool stopAcquisition();              // 停止采集

    // --------------------------- 采集 X 行自动停止 ---------------------------
    void setAcquireLineCount(int lines);  // 设置采集行数 X

signals:
    void sig_batchFinished(const HyperLineBatch &batch);// X行采集完成信号
    void sig_HSDataCallback(SI_U8* pBuffer, SI_64 frameSize, SI_64 frameNumber);

private:
    // --------------------------- 数据回调 ---------------------------
    static int dataCallback(SI_U8* pBuffer, SI_64 frameSize, SI_64 frameNumber, void* pCtx);

private:
    SI_H   m_handle = nullptr;
    bool   m_isSdkLoaded = false;
    bool   m_isOpened = false;
    bool   m_isInited = false;
    bool   m_isAcquiring = false;

    int    m_width = 0;//行宽（像素）
    int    m_bands = 224;//波段数
    SI_64  m_lineSizeBytes = 0;//单行理论字节数

    int    m_acquireLines = 10;//预设 X 行
    int    m_currentLineCount = 0;//已采集 X 行
    std::vector<unsigned char> m_batchBuffer_vecChar;//按 BIL 拼接好的 X 行数据 vecChar

private slots:
    // 实际业务处理函数
    void slot_onDataArrive(SI_U8* pBuffer, SI_64 frameSize, SI_64 frameNumber);
};

#endif // HYPERSPECTRALCAMERA_H