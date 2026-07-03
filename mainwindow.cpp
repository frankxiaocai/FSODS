#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    ,m_DeviceManager(new DeviceManager(this))
    ,m_autoTimer(new QTimer(this))
{
    ui->setupUi(this);
    init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init()
{
    //相机相关
    connect(m_DeviceManager, &DeviceManager::sig_newImage, this, [=](const QImage& img) {
        ui->label_image1->setPixmap(
            QPixmap::fromImage(img)
                .scaled(ui->label_image1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
    });

    connect(m_DeviceManager, &DeviceManager::sig_autoCaptured, this, [=](const QImage& img, QDateTime time) {
        ui->label_image2->setPixmap(
            QPixmap::fromImage(img)
                .scaled(ui->label_image2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );

        ui->label_time->setText("触发时间：" + time.toString("yyyy-MM-dd HH:mm:ss"));
    });

    // 日志
    Logger::instance()->setTextEdit(ui->textEdit);

    //高光谱相关
    connect(m_DeviceManager, &DeviceManager::sig_batchFinished, this, [=](const HyperLineBatch& LineBatch)
            {
                QDateTime currenttime = QDateTime::currentDateTime();
                QString info = QString("%1高光谱采集信息\n"
                                       "width:%2 | bands:%3 | bytesPerPixel:%4\n"
                                       "requestedLines:%5 | receivedLines:%6 | data总字节:%7\n")
                                   .arg(currenttime.toString("yyyy-MM-dd HH:mm:ss"))
                                   .arg(LineBatch.width)
                                   .arg(LineBatch.bands)
                                   .arg(LineBatch.bytesPerPixel)
                                   .arg(LineBatch.requestedLines)
                                   .arg(LineBatch.receivedLines)
                                   .arg(LineBatch.data.size());
                ui->label_lumo->setText(info);
            });

    //识别结果显示
    connect(m_DeviceManager, &DeviceManager::sig_plasticType, this, [=](int type) {
        QDateTime currenttime = QDateTime::currentDateTime();
        ui->label_type->setText(currenttime.toString("yyyy-MM-dd HH:mm:ss")+"\n塑料识别结果：" + plasticTypeToString(type));
    });

    //
    connect(m_autoTimer, &QTimer::timeout, this, &MainWindow::slot_autoCapture);
}

void MainWindow::showError(Error_code err)
{
    if (err == Error_None) {
        return; // 无错误，不弹窗
    }

    QString msg;
    switch (err) {
    case Error_None:
        return;

    case Error_ConnectFailed:
        msg = "Modbus TCP 连接失败";
        break;

    case Error_RequestFailed:
        msg = "发送 Modbus 请求失败";
        break;

    case Error_ReadRegFailed:
        msg = "读寄存器操作失败";
        break;

    case Error_WriteRegFailed:
        msg = "写寄存器操作失败";
        break;

    case Error_DeviceTestselfFailed:
        msg = "光谱仪 + 激光设备自检异常";
        break;

    case Error_InvalidData:
        msg = "写入参数数据无效";
        break;

    case Error_SingleDetectionTimeout:
        msg = "光谱仪单次检测超时（倒计时）";
        break;

    case Error_EleControl:
        msg = "电控初始化失败";
        break;

    case Error_Camera:
        msg = "相机初始化失败";
        break;

    case Error_Hyperspectral:
        msg = "高光谱初始化失败";
        break;

    case Error_Larman:
        msg = "拉曼初始化失败";
        break;

    default:
        msg = QString("未知错误码: %1").arg(err);
        break;
    }

    // 弹出错误提示框
    QMessageBox::critical(nullptr, "操作错误", msg);
}

QString MainWindow::plasticTypeToString(int code)
{
    switch (code)
    {
    case 0:  return "未知";
    case 1:  return "LDPE";
    case 2:  return "HDPE";
    case 3:  return "PP";
    case 4:  return "PS";
    case 5:  return "ABS";
    case 6:  return "PVC";
    case 7:  return "PET";
    default: return "未知";
    }
}

void MainWindow::on_pushButton_initCamera_clicked()
{
    Error_code error = m_DeviceManager->initCamera();
    if(error!= Error_None)
    {
        showError(error);
        return;
    }
}


void MainWindow::on_pushButton_initLumo_clicked()
{
    Error_code error = m_DeviceManager->initLumo();
    if(error!= Error_None)
    {
        showError(error);
        return;
    }
}


void MainWindow::on_pushButton_lumoCapture_clicked()
{
    m_DeviceManager->lumoCapture(ui->spinBox_lumo->value());
}


void MainWindow::on_pushButton_initLarman_clicked()
{
    Error_code error = m_DeviceManager->initLarman();
    if(error!= Error_None)
    {
        showError(error);
        return;
    }
}


void MainWindow::on_pushButton_LarmanCap_clicked()
{
    Error_code error = m_DeviceManager->larmanCapture();
    if(error!= Error_None)
    {
        showError(error);
        return;
    }
}

void MainWindow::on_pushButton_close_clicked()
{
    this->close();
}

void MainWindow::on_pushButton_mini_clicked()
{
    this->showMinimized();
}

void MainWindow::on_pushButton_test_clicked()
{

}

void MainWindow::on_pushButton_initEleControl_clicked()
{
    Error_code error = m_DeviceManager->initEleControl();
    if(error!= Error_None)
    {
        showError(error);
        return;
    }
    ui->label_diankong_state->setText("电控连接成功");
}


void MainWindow::on_pushButton_pushControl_1_clicked()
{
    m_DeviceManager->pushControl(1);
}


void MainWindow::on_pushButton_pushControl_2_clicked()
{
    m_DeviceManager->pushControl(2);
}

void MainWindow::slot_autoCapture()
{
    m_DeviceManager->lumoCapture(ui->spinBox_lumo->value());
}


// void MainWindow::on_pushButton_lumoCapture_auto_clicked()
// {
//     m_autoTimer->setInterval(ui->spinBox_lumo_autotime->value());
//     m_autoTimer->start();
//     qDebug() << "高光谱 已启动自动抓取";
// }


// void MainWindow::on_pushButton_lumoCapture_auto_2_clicked()
// {
//     m_autoTimer->stop();
//     qDebug() << "高光谱 已结束自动抓取";
// }


void MainWindow::on_pushButton_apply_clicked()
{
    m_DeviceManager->setType(ui->spinBox_testType->value());
    m_DeviceManager->setdelayMsL1(ui->spinBox_L1K->value());
    m_DeviceManager->setdelayMsL2(ui->spinBox_L2k->value());
    m_DeviceManager->setdelayMsL3(ui->spinBox_L3K->value());

}


void MainWindow::on_pushButton_turn_clicked()
{
    m_DeviceManager->turnControl(ui->spinBox_turn->value());
}


void MainWindow::on_checkBox_clicked(bool checked)
{
    m_DeviceManager->setIsSave(checked);
}


void MainWindow::on_pushButton_beltonoff_clicked()
{
    m_DeviceManager->beltOpen(ui->spinBox_beltnum->value(),ui->checkBox_beltonoff->checkState());
}


void MainWindow::on_pushButton_beltonoff_2_clicked()
{
    m_DeviceManager->beltSpeed(ui->spinBox_beltnum->value(),ui->spinBox_beltSpeed->value()*200);
}

