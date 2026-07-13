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

void HikCamera::processImage(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo)
{
    // if (!pData || !pFrameInfo)
    //     return;

    // unsigned int width = pFrameInfo->nWidth;
    // unsigned int height = pFrameInfo->nHeight;
    // int channel = 1;
    // MvGvspPixelType pixType = pFrameInfo->enPixelType;

    // // 匹配SDK真实枚举常量
    // if (pixType == PixelType_Gvsp_Mono8)
    // {
    //     channel = 1;
    // }
    // else if (pixType == PixelType_Gvsp_BGR8_Packed)
    // {
    //     channel = 3;
    // }
    // else if (pixType == PixelType_Gvsp_RGB8_Packed)
    // {
    //     channel = 3;
    // }
    // else
    // {
    //     std::lock_guard<std::mutex> lock(m_dataMtx);
    //     m_relX = -1.0;
    //     return;
    // }

    // // 零拷贝图像
    // cv::Mat src(height, width, channel == 3 ? CV_8UC3 : CV_8UC1, pData);
    // cv::Mat gray;
    // if (channel == 3)
    // {
    //     gray = src;
    //     // 如果是RGB格式则转BGR再灰度
    //     if (pixType == PixelType_Gvsp_RGB8_Packed)
    //         cv::cvtColor(src, src, cv::COLOR_RGB2BGR);
    //     cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    // }
    // else
    // {
    //     gray = src;
    // }

    // // 二值化分割物体
    // cv::Mat binImg;
    // cv::threshold(gray, binImg, m_binThresh, 255, cv::THRESH_BINARY_INV);

    // std::vector<std::vector<cv::Point>> contours;
    // std::vector<cv::Vec4i> hierarchy;
    // cv::findContours(binImg, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // std::vector<std::vector<cv::Point>> validTargets;
    // for (auto& cnt : contours)
    // {
    //     double area = cv::contourArea(cnt);
    //     if (area > m_minArea && area < m_maxArea)
    //     {
    //         validTargets.push_back(cnt);
    //     }
    // }

    // std::lock_guard<std::mutex> lock(m_dataMtx);
    // if (validTargets.empty())
    // {
    //     m_relX = -1.0;
    //     return;
    // }

    // // 取面积最大物体
    // auto maxCnt = *std::max_element(validTargets.begin(), validTargets.end(),
    //                                 [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b)
    //                                 {
    //                                     return cv::contourArea(a) < cv::contourArea(b);
    //                                 });

    // cv::Moments moment = cv::moments(maxCnt);
    // if (moment.m00 < 1e-6)
    // {
    //     m_relX = -1.0;
    //     return;
    // }

    // double centerX = moment.m10 / moment.m00;
    // // X相对坐标 0~1
    // m_relX = centerX / static_cast<double>(width);
}

double HikCamera::getTargetRelX()
{
    std::lock_guard<std::mutex> lock(m_dataMtx);
    return m_relX;
}

//***********************抓图逻辑  勿删！！！    勿删！！！     勿删！！！
// void __stdcall HikCamera::imageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
// {
//     HikCamera* pCamera = static_cast<HikCamera*>(pUser);
//     if (!pCamera || !pData || !pFrameInfo)
//         return;

//     // 转 Qt 图像
//     QImage currentImg(pData,
//                       pFrameInfo->nWidth,
//                       pFrameInfo->nHeight,
//                       QImage::Format_Grayscale8);

//     // ==============================
//     // 传送带自动抓图
//     // ==============================
//     if (pCamera->m_autoCapture)
//     {
//         if (!pCamera->m_lastImage.isNull() && !pCamera->m_hasTriggered)
//         {
//             // ========== 传送带专用参数 ==========
//             const int THRESHOLD      = 10;   // 灵敏度
//             const int TRIGGER_PIXELS = 800;  // 触发大小
//             int diffCount = 0;

//             int skip = 2;
//             int w = currentImg.width();
//             int h = currentImg.height();

//             for (int y = 0; y < h && diffCount <= TRIGGER_PIXELS; y += skip)
//             {
//                 const uchar* currLine = currentImg.constScanLine(y);
//                 const uchar* lastLine = pCamera->m_lastImage.constScanLine(y);

//                 for (int x = 0; x < w && diffCount <= TRIGGER_PIXELS; x += skip)
//                 {
//                     int delta = qAbs(currLine[x] - lastLine[x]);
//                     if (delta > THRESHOLD)
//                         diffCount++;
//                 }
//             }

//             // 触发抓图
//             if (diffCount > TRIGGER_PIXELS)
//             {

//                 emit pCamera->sig_autoCaptured(currentImg.copy(),QDateTime::currentDateTime());

//                 // ==============================
//                 // 自动重置：抓图后延迟重置
//                 // ==============================
//                 pCamera->m_hasTriggered = true;
//                 const int trigtime = 2000;   // 延迟时间 毫秒
//                 QTimer::singleShot(trigtime, pCamera, [pCamera]() {
//                     pCamera->m_hasTriggered = false;
//                 });
//             }
//         }

//         pCamera->m_lastImage = currentImg.copy();
//     }

//     emit pCamera->sig_newImage(currentImg.copy());//帧流
// }

void __stdcall HikCamera::imageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    if (nullptr == pUser || nullptr == pData || nullptr == pFrameInfo)
        return;

    // pUser传入HikCamera实例指针，转发处理
    HikCamera* pCam = static_cast<HikCamera*>(pUser);
    pCam->processImage(pData, pFrameInfo);
}
