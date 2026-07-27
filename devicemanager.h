#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMessageBox>
#include <QtConcurrent>
#include "./Devices/hikcamera.h"
#include "./Devices/hyperspectralcamera.h"
#include "./Devices/larmanmodbustcp.h"
#include "./Devices/plccontroller.h"
#include "./CoreTools/fileio.h"
#include "./CoreTools/logger.h"
#include "./CoreTools/RamanPlasticRecognizer.h"
#include "./CoreTools/HSIPlasticRecognizer/HSIProcessor.h"

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    void init();
    Error_code initCamera();
    Error_code initLumo();
    Error_code initLarman();
    Error_code initEleControl();

    //采集
    void HIKCapture();
    Error_code lumoCapture(int XNum);
    Error_code larmanCapture();

    //参数设置
    void setdelayMsL0(int lt){m_delayMsL0 = lt;}
    void setdelayMsL1(int lt){m_delayMsL1 = lt;}
    void setdelayMsL2(int lt){m_delayMsL2 = lt;}
    void setdelayMsL3(int lt){m_delayMsL3 = lt;}
    void setdelayMsL4(int lt){m_delayMsL4 = lt;}
    void setlarmanDelay(int lt){m_larmanDelay = lt;}
    void setIsSave(bool aaa){m_isSave = aaa;}
    void setExposure(double aaa);
    void setFrameRate(double aaa);
    void setXLines(int line){m_XLines = line;}

    //制动控制
    void beltOpen(int num,bool isopen);
    void beltSpeed(int num,int speed);
    void pushControl(int num,bool op);//推杆 （序号）
    void turnControl(int num,int order);

    // 物体计数
    void updateObjectCount(int objType);
    int getObjTypeCount(int type);
    int getObjTotalCount();
    void clearAllObjectCount();

    void testcount();

private:
    HikCamera* m_HikCamera = nullptr;
    HyperspectralCamera* m_HyperspectralCamera = nullptr;
    LarmanModbusTCP* m_larmanModbusTCP = nullptr;
    PlcController* m_siemensModbusPlc = nullptr;
    HSIProcessor m_HSIClassifier;//HSI塑料分类算法
    RamanPlasticRecognizer m_RamanPlasticRecognizer;//拉曼塑料分类算法

    //高光谱参数
    double m_Exposure = 10;//曝光时间 ms
    double m_FrameRate = 200;//帧率
    int m_XLines = 40;//采集行数
    bool m_isSave = false;//标识位

    //制动延迟
    int m_delayMsL1 = 1000;//1号制动延迟 ms
    int m_delayMsL2 = 2000;
    int m_delayMsL3 = 3000;
    int m_delayMsL4 = 4000;
    int m_delayMsL0 = 0;//光栅至高光谱延迟 ms
    int m_larmanDelay = 900;//拉曼单独控制逻辑延迟差

    //物体计数
    int m_objCount[8] = {0}; // 1~7种塑料 + 未知
    int m_objTotal = 0;//总数

private:
    void writeBatch2Raw(const HyperLineBatch &batch,int type);//保存采集光谱+类型数据
    QImage Mat2QImage(const cv::Mat &mat);
    void lamanActControl(int type);//制动-拉曼单独一套控制逻辑

private slots:
    void slot_onObjectArrived();//光栅检测物体到达处理
    void slot_onFrameArrived(const HyperLineBatch &batch);//高光谱采集结果处理
    void slot_onHikCaptureArrived(cv::Mat targetOnly);//相机定位图像处理
    void slot_hikObjectXY(double X,double Y); //相机定位位置处理
    void slot_actControl(int type);//制动

signals:
    void sig_newImage(const QImage& img);//相机图像流
    void sig_hikCaptured(const QImage& img); //相机定位图像
    void sig_hikObjectXY(double X,double Y); //相机定位位置
    void sig_batchFinished(const HyperLineBatch &batch);//高光谱采集结果
    void sig_plasticType(int type);// 塑料识别结果信号
};

#endif // DEVICEMANAGER_H
