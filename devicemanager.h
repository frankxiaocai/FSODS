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
    Error_code lumoCapture(int XNum);
    Error_code larmanCapture();

    void setType(int type){m_testType = type;}
    void setdelayMsL1(int lt){m_delayMsL1 = lt;}
    void setdelayMsL2(int lt){m_delayMsL2 = lt;}
    void setdelayMsL3(int lt){m_delayMsL3 = lt;}
    void setIsSave(bool aaa){m_isSave = aaa;}
    void setExposure(double aaa);
    void setFrameRate(double aaa);
    void setXLines(int line){m_XLines = line;}

    //制动控制
    void beltOpen(int num,bool isopen);
    void beltSpeed(int num,int speed);
    void pushControl(int num,bool op);//推杆 （序号）
    void turnControl(int order);

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
    int m_testType = 1;//类型 测试用
    int m_delayMsL1 = 1000;//1号制动延迟 ms
    int m_delayMsL2 = 2000;
    int m_delayMsL3 = 3000;

    //物体计数
    int m_objCount[8] = {0}; // 1~7种塑料 + 未知
    int m_objTotal = 0;//总数

private:
    void writeBatch2Raw(const HyperLineBatch &batch,int type);

private slots:
    void slot_onWasteArrived();
    void slot_onFrameArrived(const HyperLineBatch &batch);
    void slot_onHikCaptureArrived(cv::Mat targetOnly);

signals:
    void sig_newImage(const QImage& img);//图像流
    void sig_hikCaptured(const QImage& img); //相机抓图
    void sig_batchFinished(const HyperLineBatch &batch);  // X 行高光谱
    void sig_plasticType(int type);// 塑料识别结果信号
};

#endif // DEVICEMANAGER_H
