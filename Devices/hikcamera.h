#ifndef HIKCAMERA_H
#define HIKCAMERA_H

#include <QObject>
#include <QImage>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QCoreApplication>
#include "MvCameraControl.h"
#include <opencv2/opencv.hpp>

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

    void hikOnceCapture(){m_captureFlag = true;}//相机抓图
    void objectLocate(cv::Mat gray,cv::Mat& targetOnly,double& normX);

public:
    void* m_handle = nullptr;//句柄

private:
    static void __stdcall imageCallback(unsigned char* pData,MV_FRAME_OUT_INFO_EX* pFrameInfo,void* pUser);
    void processImage(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo);// 回调转发到成员函数处理图像

    // 定位相关
    bool m_captureFlag = false; // 抓拍触发标记
    double m_relX = -1.0;       // 归一化相对 X 坐标

signals:
    void sig_newImage(const QImage& img);//图像流
    void sig_objectLocation(double relX);
    void sig_objectCapture(cv::Mat mergeMat);
};

#endif // HIKCAMERA_H
