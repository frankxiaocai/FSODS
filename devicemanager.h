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

    Error_code lumoCapture(int XNum);
    Error_code larmanCapture();

    void pushControl(int num);

private:
    HikCamera* m_HikCamera = nullptr;
    HyperspectralCamera* m_HyperspectralCamera = nullptr;
    LarmanModbusTCP* m_larmanModbusTCP = nullptr;
    PlcController* m_siemensModbusPlc = nullptr;
    HSIProcessor m_HSIClassifier;//HSI塑料分类算法
    RamanPlasticRecognizer m_RamanPlasticRecognizer;//拉曼塑料分类算法
    otherConfigs m_Configs;//配置参数

    double m_Exposure = 10;//曝光时间 ms
    double m_FrameRate = 200;//帧率

    bool m_error_simens;


private slots:
    void slot_onFrameArrived(const HyperLineBatch &batch);

signals:
    void sig_newImage(const QImage& img);//图像流
    void sig_autoCaptured(const QImage& img,QDateTime time); //相机抓图
    void sig_batchFinished(const HyperLineBatch &batch);  // X 行高光谱
    void sig_plasticType(int type);// 塑料识别结果信号

};

#endif // DEVICEMANAGER_H
