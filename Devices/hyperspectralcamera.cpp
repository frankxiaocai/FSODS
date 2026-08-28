
#include "HyperspectralCamera.h"
#include <QDebug>

HyperspectralCamera::HyperspectralCamera(QObject *parent) : QObject(parent)
{
    connect(this,&HyperspectralCamera::sig_HSDataCallback,this,&HyperspectralCamera::slot_onDataArrive);
}
HyperspectralCamera::~HyperspectralCamera()
{
    stopAcquisition();
    closeDevice();
    unloadSDK();
    qDebug() << "高光谱已安全销毁 析构释放";

}

int HyperspectralCamera::setSSP()
{
    int st = SI_SetString(SI_SYSTEM, const_cast<SI_WC*>(L"ProfilesDirectory"), const_cast<SI_WC*>(L"./SSP"));
    return st;
}

// ========================== 加载 SDK（官方 9.2）==========================
bool HyperspectralCamera::loadSDK()
{
    if (m_isSdkLoaded) return true;

    // 返回 0 表示成功
    int st = SI_Load(L"");

    if (st != 0) {
        return false;
    }

    m_isSdkLoaded = true;

    return true;
}

bool HyperspectralCamera::unloadSDK() {
    if (!m_isSdkLoaded) return true;
    SI_Unload();
    m_isSdkLoaded = false;

    return true;
}

// ========================== 设备枚举（官方 9.1）==========================
int HyperspectralCamera::deviceCount() {
    if (!m_isSdkLoaded) return -1;
    SI_64 cnt = 0;
    SI_GetInt(SI_SYSTEM, L"DeviceCount", &cnt);
    return (int)cnt;
}

QString HyperspectralCamera::deviceName(int index) {
    if (!m_isSdkLoaded) return {};
    wchar_t buf[256] = {0};
    SI_GetEnumStringByIndex(SI_SYSTEM, L"DeviceName", index, buf, 256);
    return QString::fromWCharArray(buf);
}

// ========================== 打开设备（官方 9.2）==========================
bool HyperspectralCamera::openDevice(int index) {
    if (!m_isSdkLoaded || m_isOpened) return false;
    int st = SI_Open(index, &m_handle);
    if (st != 0) {

        return false;
    }
    m_isOpened = true;
    return true;
}

bool HyperspectralCamera::closeDevice() {
    if (!m_isOpened) return true;
    stopAcquisition();
    SI_Close(m_handle);
    m_handle = nullptr;
    m_isOpened = false;
    m_isInited = false;

    return true;
}

// ========================== 初始化（官方 9.2）==========================
bool HyperspectralCamera::initialize() {
    if (!m_isOpened || m_isInited) return false;
    int st = SI_Command(m_handle, L"Initialize");
    if (st != 0) {
        return false;
    }
    m_isInited = true;

    // 获取行宽、波段数、单行字节数
    SI_GetInt(m_handle, L"Camera.Image.Width", (SI_64*)&m_width);
    SI_GetEnumCount(m_handle, L"Camera.WavelengthTable", &m_bands);
    m_lineSizeBytes = m_width * m_bands * 2;

    return true;
}

// ========================== 参数设置 ==========================
bool HyperspectralCamera::setExposure(double ms) {
    return SI_SetFloat(m_handle, L"Camera.ExposureTime", ms) == 0;
}
bool HyperspectralCamera::setFrameRate(double fps) {
    return SI_SetFloat(m_handle, L"Camera.FrameRate", fps) == 0;
}
bool HyperspectralCamera::setSpatialBin(int bin) {
    return SI_SetEnumIndex(m_handle, L"Camera.Binning.Spatial", bin) == 0;
}
bool HyperspectralCamera::setSpectralBin(int bin) {
    return SI_SetEnumIndex(m_handle, L"Camera.Binning.Spectral", bin) == 0;
}

// ========================== 设置要采集的行数 X ==========================
void HyperspectralCamera::setAcquireLineCount(int lines) {
    if (lines > 0) m_acquireLines = lines;
}

// ========================== 启动采集 ==========================
bool HyperspectralCamera::startAcquisition() {
    if (!m_isInited || m_isAcquiring) return false;

    m_batchBuffer_vecChar.clear();
    m_currentLineCount = 0;
    // 注册回调函数
    int st = SI_RegisterDataCallback(m_handle, dataCallback, this);
    if (st != 0) return false;

    // 官方 9.2 启动采集
    st = SI_Command(m_handle, L"Acquisition.Start");
    if (st != 0) {
        SI_UnregisterDataCallback(m_handle);
        return false;
    }

    m_isAcquiring = true;
    return true;
}

// ========================== 停止 ==========================
bool HyperspectralCamera::stopAcquisition() {
    if (!m_isAcquiring) return true;

    SI_Command(m_handle, L"Acquisition.Stop");
    SI_UnregisterDataCallback(m_handle);
    m_isAcquiring = false;
    return true;
}

// ========================== 回调函数：每来 1 行调用 1 次 ==========================
/**
 * @brief HyperspectralCamera::dataCallback
 * @param pBuffer  数据流 （BIL规则排列）
 * @param frameSize 每帧字节数
 * @param frameNumber 第几帧
 * @param pCtx this指针
 * @return
 */
int HyperspectralCamera::dataCallback(SI_U8* pBuffer, SI_64 frameSize, SI_64 frameNumber, void* pCtx)
{

    HyperspectralCamera* cam = reinterpret_cast<HyperspectralCamera*>(pCtx);
    cam->emit sig_HSDataCallback(pBuffer,frameSize,frameNumber);

    // auto& buf = cam->m_batchBuffer_vecChar;
    // size_t oldSize = buf.size();
    // buf.reserve(oldSize + frameSize);// 预分配容量
    // buf.resize(oldSize + frameSize);// 扩大容器有效长度
    // std::memcpy(&buf[oldSize], pBuffer, frameSize);// 直接内存拷贝

    // cam->m_currentLineCount++;
    // if (cam->m_currentLineCount >= cam->m_acquireLines)
    // {
    //     HyperLineBatch batch;
    //     batch.data = cam->m_batchBuffer_vecChar;
    //     batch.width = cam->m_width;
    //     batch.bands = cam->m_bands;
    //     batch.requestedLines = cam->m_acquireLines;
    //     batch.receivedLines = cam->m_currentLineCount;

    //     emit cam->sig_batchFinished(batch);
    //     cam->stopAcquisition();
    // }

    return 0;

}

void HyperspectralCamera::slot_onDataArrive(SI_U8 *pBuffer, SI_64 frameSize, SI_64 frameNumber)
{
    auto& buf = m_batchBuffer_vecChar;
    size_t oldSize = buf.size();
    buf.reserve(oldSize + frameSize);// 预分配容量
    buf.resize(oldSize + frameSize);// 扩大容器有效长度
    std::memcpy(&buf[oldSize], pBuffer, frameSize);// 直接内存拷贝

    m_currentLineCount++;
    if (m_currentLineCount >= m_acquireLines)
    {
        HyperLineBatch batch;
        batch.data = m_batchBuffer_vecChar;
        batch.width = m_width;
        batch.bands = m_bands;
        batch.requestedLines = m_acquireLines;
        batch.receivedLines = m_currentLineCount;

        emit sig_batchFinished(batch);
        stopAcquisition();

        m_currentLineCount = 0;
        m_batchBuffer_vecChar.clear();
    }

    return ;
}