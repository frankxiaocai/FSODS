#ifndef HIKCAMERA_H
#define HIKCAMERA_H

#include <QObject>
#include <QImage>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QCoreApplication>
#include "../CoreTools/mystruct.h"
#include "MvCameraControl.h"

class HikCamera : public QObject
{
    Q_OBJECT
public:
    explicit HikCamera(QObject *parent = nullptr);
    ~HikCamera();

    bool enumDevices();//枚举设备
    bool openDevice(int index);//打开设备
    bool startGrabbing();//开始采集
    void stopGrabbing();//停止采集
    void closeDevice();//关闭设备

    // 自动抓图开关
    void enableAutoCapture(bool enable) { m_autoCapture = enable; }

    // 定位相关
    // 回调转发到成员函数处理图像
    void processImage(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo);

    // 获取最新物体X相对坐标
    double getTargetRelX();

public:
    void* m_handle = nullptr;//句柄


private:
    static void __stdcall imageCallback(unsigned char* pData,MV_FRAME_OUT_INFO_EX* pFrameInfo,void* pUser);

    bool m_autoCapture = true;// 是否开启自动抓图
    QImage m_lastImage;// 自动抓图最近一帧
    bool m_hasTriggered = false;// 防止重复触发

    // 定位相关
    std::mutex m_dataMtx;
    double m_relX = -1.0;  // -1代表无有效物体

    // 图像处理参数（可外部配置）
    int m_binThresh = 120;    // 二值化阈值
    int m_minArea = 500;      // 最小物体面积，过滤噪点
    int m_maxArea = 100000;   // 最大物体面积

signals:
    void sig_newImage(const QImage& img);//图像流
    void sig_autoCaptured(const QImage& img,QDateTime time); // 抓图结果
};

#endif // HIKCAMERA_H
