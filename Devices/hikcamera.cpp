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

// void HikCamera::objectLocate(cv::Mat gray, cv::Mat &targetOnly, double &normX, cv::Mat& drawOut)
// {
//     // 初始化输出
//     double centerX = 0.0;
//     double centerY = 0.0;
//     normX = 0.0;
//     targetOnly = cv::Mat::zeros(gray.size(), CV_8UC1);
//     drawOut.release();

//     // 空图像保护
//     if (gray.empty())
//         return;

//     int imgWidth = gray.cols;
//     int imgHeight = gray.rows;
//     if (imgWidth <= 0 || imgHeight <= 0)
//         return;

//     cv::Mat binImg;
//     // OTSU自动阈值分割
//     cv::threshold(gray, binImg, 60, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

//     // 形态学去噪
//     cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
//     cv::morphologyEx(binImg, binImg, cv::MORPH_OPEN, kernel);

//     // 查找轮廓
//     std::vector<std::vector<cv::Point>> contours;
//     std::vector<cv::Vec4i> hierarchy;
//     cv::findContours(binImg, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

//     // 先把灰度转彩色，用于绘图
//     cv::cvtColor(gray, drawOut, cv::COLOR_GRAY2BGR);
//     cv::Scalar red = cv::Scalar(0, 0, 255); // BGR红色

//     if (contours.empty())
//         return;

//     // 取面积最大轮廓
//     auto maxContourIt = std::max_element(contours.begin(), contours.end(),
//                                          [](const std::vector<cv::Point>& c1, const std::vector<cv::Point>& c2)
//                                          {
//                                              return cv::contourArea(c1) < cv::contourArea(c2);
//                                          });
//     std::vector<cv::Point> targetContour = *maxContourIt;

//     // 生成目标掩码图像
//     cv::drawContours(targetOnly, std::vector<std::vector<cv::Point>>{targetContour},
//                      0, cv::Scalar(255), cv::FILLED);

//     // 计算重心像素坐标
//     cv::Moments moments = cv::moments(targetContour);
//     if (moments.m00 > 1e-6)
//     {
//         centerX = moments.m10 / moments.m00;
//         centerY = moments.m01 / moments.m00;
//         // 归一化：重心X / 图像总宽度，范围 [0, 1]
//         normX = centerX / (double)imgWidth;

//         // 重心点转为整型坐标
//         cv::Point centerPt(cvRound(centerX), cvRound(centerY));
//         int crossLen = 15; // 十字线长度，可自行调整

//         // 绘制水平横线
//         cv::line(drawOut, cv::Point(centerPt.x - crossLen, centerPt.y),
//                  cv::Point(centerPt.x + crossLen, centerPt.y), red, 2);
//         // 绘制垂直竖线
//         cv::line(drawOut, cv::Point(centerPt.x, centerPt.y - crossLen),
//                  cv::Point(centerPt.x, centerPt.y + crossLen), red, 2);
//         // 中心实心圆点，突出重心
//         cv::circle(drawOut, centerPt, 4, red, -1);

//     }
// }
void HikCamera::objectLocate(cv::Mat gray, cv::Mat& targetOnly, double& normX, double& normY, cv::Mat& drawOut)
{
    // 初始化输出
    double centerX = 0.0;
    double centerY = 0.0;
    normX = 0.0;
    normY = 0.0;
    targetOnly = cv::Mat::zeros(gray.size(), CV_8UC1);
    drawOut.release();

    // 空图像保护
    if (gray.empty())
        return;

    int imgWidth = gray.cols;
    int imgHeight = gray.rows;
    if (imgWidth <= 0 || imgHeight <= 0)
        return;

    cv::Mat binImg;
    // OTSU自动阈值分割
    cv::threshold(gray, binImg, 60, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    // 形态学去噪
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(binImg, binImg, cv::MORPH_OPEN, kernel);

    // 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(binImg, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 灰度转BGR彩色图用于绘制
    cv::cvtColor(gray, drawOut, cv::COLOR_GRAY2BGR);
    cv::Scalar red = cv::Scalar(0, 0, 255); // BGR红色

    if (contours.empty())
        return;

    // 取面积最大轮廓
    auto maxContourIt = std::max_element(contours.begin(), contours.end(),
                                         [](const std::vector<cv::Point>& c1, const std::vector<cv::Point>& c2)
                                         {
                                             return cv::contourArea(c1) < cv::contourArea(c2);
                                         });
    std::vector<cv::Point> targetContour = *maxContourIt;

    // 生成目标掩码图像
    cv::drawContours(targetOnly, std::vector<std::vector<cv::Point>>{targetContour},
                     0, cv::Scalar(255), cv::FILLED);

    // 计算重心像素坐标
    cv::Moments moments = cv::moments(targetContour);
    if (moments.m00 > 1e-6)
    {
        centerX = moments.m10 / moments.m00;
        centerY = moments.m01 / moments.m00;
        normX = centerX / (double)imgWidth;
        normY = centerY / (double)imgHeight;

        // 重心整型坐标
        cv::Point centerPt(cvRound(centerX), cvRound(centerY));
        int crossLen = 15;       // 十字线长度
        int textOffsetX = 20;    // 文字向右偏移
        int textOffsetY = -15;   // 文字向上偏移，放在十字右上方

        // 绘制水平横线
        cv::line(drawOut, cv::Point(centerPt.x - crossLen, centerPt.y),
                 cv::Point(centerPt.x + crossLen, centerPt.y), red, 2);
        // 绘制垂直竖线
        cv::line(drawOut, cv::Point(centerPt.x, centerPt.y - crossLen),
                 cv::Point(centerPt.x, centerPt.y + crossLen), red, 2);
        // 中心实心红点
        cv::circle(drawOut, centerPt, 4, red, -1);

        // 拼接显示文本，保留4位小数
        char textBuf[128] = {0};
        sprintf(textBuf, "X:%.3f  Y:%.3f", normX, normY);
        cv::Point textPos(centerPt.x + textOffsetX, centerPt.y + textOffsetY);

        // 绘制红色文字
        cv::putText(drawOut, textBuf, textPos,
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,    // 字体缩放大小
                    red,    // 红色
                    1);     // 文字线条粗细
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
            m_normX = -1.0;
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
        cv::Mat mergeMat;
        cv::Mat drawMat;
        objectLocate(gray,mergeMat,m_normX,m_normY,drawMat);
        emit sig_objectCapture(drawMat);
        emit sig_objectLocation(m_normX,m_normY);
    }

    // 转 Qt 图像
    QImage currentImg(pData,
                      pFrameInfo->nWidth,
                      pFrameInfo->nHeight,
                      QImage::Format_Grayscale8);
    emit sig_newImage(currentImg.copy());//帧流

}

void __stdcall HikCamera::imageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    if (nullptr == pUser || nullptr == pData || nullptr == pFrameInfo)
        return;

    // pUser传入HikCamera实例指针，转发处理
    HikCamera* pCam = static_cast<HikCamera*>(pUser);
    pCam->processImage(pData, pFrameInfo);

}
