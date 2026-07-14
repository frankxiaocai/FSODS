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

    // 注册图像回调
    nRet = MV_CC_RegisterImageCallBackEx(m_handle, imageCallback, this);
    if (nRet != MV_OK) {
        qDebug() << "注册回调失败：" << nRet;
        return false;
    }
    return true;
}

bool HikCamera::startGrabbing()
{
    if (!m_handle) return false;

    int nRet = MV_CC_StartGrabbing(m_handle);
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
    if (!pData || !pFrameInfo)
        return;

    if(m_captureFlag)//抓图
    {
        m_captureFlag = false;//只抓一次

        unsigned int width = pFrameInfo->nWidth;
        unsigned int height = pFrameInfo->nHeight;
        int channel = 1;
        MvGvspPixelType pixType = pFrameInfo->enPixelType;

        if (pixType == PixelType_Gvsp_Mono8)
        {
            channel = 1;
        }
        else if (pixType == PixelType_Gvsp_BGR8_Packed)
        {
            channel = 3;
        }
        else if (pixType == PixelType_Gvsp_RGB8_Packed)
        {
            channel = 3;
        }
        else
        {
            m_relX = -1.0;
            return;
        }

        // 零拷贝图像 统一转灰度图
        cv::Mat src(height, width, channel == 3 ? CV_8UC3 : CV_8UC1, pData);
        cv::Mat gray;
        if (channel == 3)
        {
            gray = src;
            if (pixType == PixelType_Gvsp_RGB8_Packed)
                cv::cvtColor(src, src, cv::COLOR_RGB2BGR);
            cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
        }
        else
        {
            gray = src;
        }

        //计算物体质心像素
        cv::Mat targetOnly;
        double centerX = 0;
        objectLocate(gray,targetOnly,centerX);
        emit sig_objectCapture(targetOnly);

        // 归一化相对 X 坐标
        m_relX = (centerX)/(static_cast<double>(width));//物体贴图像最左侧：m_relX ≈ 0
        emit sig_objectLocation(m_relX);
    }

    // 转 Qt 图像
    QImage currentImg(pData,
                      pFrameInfo->nWidth,
                      pFrameInfo->nHeight,
                      QImage::Format_Grayscale8);
    emit sig_newImage(currentImg.copy());//帧流

}

void HikCamera::objectLocate(cv::Mat gray, cv::Mat& targetOnly,double& centerX)
{
    // 二值化分割物体
    cv::Mat binImg;
    cv::threshold(gray, binImg, m_binThresh, 255, cv::THRESH_BINARY_INV);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(binImg, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<std::vector<cv::Point>> validTargets;
    for (auto& cnt : contours)
    {
        double area = cv::contourArea(cnt);
        if (area > m_minArea && area < m_maxArea)
        {
            validTargets.push_back(cnt);
        }
    }

    if (validTargets.empty())
    {
        m_relX = -1.0;
        return;
    }

    // 取面积最大物体
    auto maxCnt = *std::max_element(validTargets.begin(), validTargets.end(),
                                    [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b)
                                    {
                                        return cv::contourArea(a) < cv::contourArea(b);
                                    });

    // ========== 生成掩码，只保留目标物体，背景全黑 ==========
    cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::drawContours(mask, std::vector<std::vector<cv::Point>>{maxCnt}, 0, cv::Scalar(255), -1);
    gray.copyTo(targetOnly, mask);

    //矩运算计算物体质心
    cv::Moments moment = cv::moments(maxCnt);
    if (moment.m00 < 1e-6)
    {
        m_relX = -1.0;
        return;
    }

    //质心 X 像素坐标 (0.0 ≤ centerX ≤ width - 1)
    centerX = moment.m10 / moment.m00;

}

void __stdcall HikCamera::imageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    if (nullptr == pUser || nullptr == pData || nullptr == pFrameInfo)
        return;

    // pUser传入HikCamera实例指针，转发处理
    HikCamera* pCam = static_cast<HikCamera*>(pUser);
    pCam->processImage(pData, pFrameInfo);

}
