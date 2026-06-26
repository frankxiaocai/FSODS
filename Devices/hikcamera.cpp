#include "hikcamera.h"
#include <QDebug>

HikCamera::HikCamera(QObject *parent)
    : QObject(parent)
{
}

HikCamera::~HikCamera()
{
    // 停止采集
    stopGrabbing();
    // 关闭设备并释放句柄
    closeDevice();
    qDebug() << "相机已安全销毁 析构释放";
}

bool HikCamera::enumDevices()
{
    MV_CC_DEVICE_INFO_LIST stDevList;
    memset(&stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDevList);
    if (nRet != MV_OK) {
        qDebug() << "枚举设备失败，错误码：" << nRet;
        return false;
    }

    qDebug() << "找到设备数量：" << stDevList.nDeviceNum;
    for (unsigned int i = 0; i < stDevList.nDeviceNum; ++i) {
        MV_CC_DEVICE_INFO* pInfo = stDevList.pDeviceInfo[i];
        if (pInfo) {
            qDebug() << "设备" << i << ":"
                     << (pInfo->nTLayerType == MV_GIGE_DEVICE ? "GigE" : "USB");
        }
    }
    return stDevList.nDeviceNum > 0;
}

bool HikCamera::openDevice(int index)
{
    if (m_handle) {
        closeDevice();
    }

    MV_CC_DEVICE_INFO_LIST stDevList;
    MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDevList);

    if (index < 0 || index >= stDevList.nDeviceNum) {
        qDebug() << "设备索引无效";
        return false;
    }

    // 创建句柄
    int nRet = MV_CC_CreateHandle(&m_handle, stDevList.pDeviceInfo[index]);
    if (nRet != MV_OK) {
        qDebug() << "创建句柄失败：" << nRet;
        return false;
    }

    // 打开设备
    nRet = MV_CC_OpenDevice(m_handle);
    if (nRet != MV_OK) {
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        qDebug() << "打开设备失败：" << nRet;
        return false;
    }

    // 设置为连续采集模式
    MV_CC_SetEnumValue(m_handle, "TriggerMode", 0);
    return true;
}

bool HikCamera::startGrabbing()
{
    if (!m_handle) return false;

    // 注册图像回调
    int nRet = MV_CC_RegisterImageCallBackEx(m_handle, imageCallback, this);
    if (nRet != MV_OK) {
        qDebug() << "注册回调失败：" << nRet;
        return false;
    }

    // 开始采集
    nRet = MV_CC_StartGrabbing(m_handle);
    if (nRet != MV_OK) {
        qDebug() << "开始采集失败：" << nRet;
        return false;
    }
    return true;
}

void HikCamera::stopGrabbing()
{
    if (m_handle)
    {
        MV_CC_StopGrabbing(m_handle);
    }
}

void HikCamera::closeDevice()
{
    if (m_handle)
    {
        MV_CC_StopGrabbing(m_handle);
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
    }
}


void __stdcall HikCamera::imageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    HikCamera* pCamera = static_cast<HikCamera*>(pUser);
    if (!pCamera || !pData || !pFrameInfo)
        return;

    // 转 Qt 图像
    QImage currentImg(pData,
                      pFrameInfo->nWidth,
                      pFrameInfo->nHeight,
                      QImage::Format_Grayscale8);

    // ==============================
    // 传送带自动抓图（自动重置版）
    // ==============================
    if (pCamera->m_autoCapture)
    {
        if (!pCamera->m_lastImage.isNull() && !pCamera->m_hasTriggered)
        {
            // ========== 传送带专用参数 ==========
            const int THRESHOLD      = 10;   // 灵敏度
            const int TRIGGER_PIXELS = 800;  // 触发大小
            int diffCount = 0;

            int skip = 2;
            int w = currentImg.width();
            int h = currentImg.height();

            for (int y = 0; y < h && diffCount <= TRIGGER_PIXELS; y += skip)
            {
                const uchar* currLine = currentImg.constScanLine(y);
                const uchar* lastLine = pCamera->m_lastImage.constScanLine(y);

                for (int x = 0; x < w && diffCount <= TRIGGER_PIXELS; x += skip)
                {
                    int delta = qAbs(currLine[x] - lastLine[x]);
                    if (delta > THRESHOLD)
                        diffCount++;
                }
            }

            // 触发抓图
            if (diffCount > TRIGGER_PIXELS)
            {

                emit pCamera->sig_autoCaptured(currentImg.copy(),QDateTime::currentDateTime());

                // ==============================
                // 自动重置：抓图后延迟重置
                // ==============================
                pCamera->m_hasTriggered = true;
                const int trigtime = 2000;   // 延迟时间 毫秒
                QTimer::singleShot(trigtime, pCamera, [pCamera]() {
                    pCamera->m_hasTriggered = false;
                });
            }
        }

        pCamera->m_lastImage = currentImg.copy();
    }

    emit pCamera->sig_newImage(currentImg.copy());//帧流
}

