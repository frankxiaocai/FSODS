#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    ,m_DeviceManager(new DeviceManager(this))
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
    initUI();
    //相机相关
    connect(m_DeviceManager, &DeviceManager::sig_newImage, this, [=](const QImage& img) {
        ui->label_image1->setPixmap(
            QPixmap::fromImage(img)
                .scaled(ui->label_image1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
    });

    connect(m_DeviceManager, &DeviceManager::sig_hikCaptured, this, [=](const QImage& img) {
        ui->label_image2->setPixmap(
            QPixmap::fromImage(img)
                .scaled(ui->label_image2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
    });

    connect(m_DeviceManager, &DeviceManager::sig_hikObjectXY, this, [=](double X, double Y)
            {
                QString text = QString("normX = %1     normY = %2").arg(X, 0, 'f', 3).arg(Y, 0, 'f', 3);
                ui->label_objectXY->setText(text);
            });

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
        //计数
        updateCountLabel(type);

    });

}

void MainWindow::initUI()
{
    diankongConfigs dkc;
    FileIO::instance()->readDianKongConfig(dkc);
    setDiankongConfigs(dkc);

    // 日志
    Logger::instance()->setTextEdit(ui->textEdit);
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

void MainWindow::updateCountLabel(int type)
{
    m_DeviceManager->updateObjectCount(type);
    switch (type)
    {
    case 0:
        ui->label_type00->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;

    case 1:
        ui->label_type11->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;

    case 2:
        ui->label_type22->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;

    case 3:
        ui->label_type33->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;

    case 4:
        ui->label_type44->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;

    case 5:
        ui->label_type55->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;

    case 6:
        ui->label_type66->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;

    case 7:
        ui->label_type77->setText(QString::number(m_DeviceManager->getObjTypeCount(type)));
        break;
    }

    ui->label_typeAll2->setText(QString::number(m_DeviceManager->getObjTotalCount()));


}

diankongConfigs MainWindow::getDiankongConfigs()
{
    diankongConfigs dkConfigs;
    dkConfigs.delayMsL0 = ui->spinBox_L0K->value();
    dkConfigs.delayMsL1 = ui->spinBox_L1K->value();
    dkConfigs.delayMsL2 = ui->spinBox_L2k->value();
    dkConfigs.delayMsL3 = ui->spinBox_L3K->value();
    dkConfigs.delayMsL4 = ui->spinBox_L4K->value();
    dkConfigs.delayMsLarman = ui->spinBox_larmandelay->value();
    dkConfigs.Exposure = ui->spinBox_baoguang->value();
    dkConfigs.FrameRate = ui->spinBox_zhenlv->value();
    dkConfigs.XLines = ui->spinBox_lumo->value();
    return dkConfigs;
}

void MainWindow::setDiankongConfigs(diankongConfigs dk)
{
    ui->spinBox_L0K->setValue(dk.delayMsL0);
    ui->spinBox_L1K->setValue(dk.delayMsL1);
    ui->spinBox_L2k->setValue(dk.delayMsL2);
    ui->spinBox_L3K->setValue(dk.delayMsL3);
    ui->spinBox_L4K->setValue(dk.delayMsL4);
    ui->spinBox_larmandelay->setValue(dk.delayMsLarman);
    ui->spinBox_baoguang->setValue(dk.Exposure);
    ui->spinBox_zhenlv->setValue(dk.FrameRate);
    ui->spinBox_lumo->setValue(dk.XLines);
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
    m_DeviceManager->testcount();
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
    m_DeviceManager->pushControl(1,true);
}


void MainWindow::on_pushButton_pushControl_2_clicked()
{
    m_DeviceManager->pushControl(2,true);
}

void MainWindow::on_pushButton_apply_clicked()
{
    m_DeviceManager->setdelayMsL0(ui->spinBox_L0K->value());
    m_DeviceManager->setdelayMsL1(ui->spinBox_L1K->value());
    m_DeviceManager->setdelayMsL2(ui->spinBox_L2k->value());
    m_DeviceManager->setdelayMsL3(ui->spinBox_L3K->value());
    m_DeviceManager->setdelayMsL4(ui->spinBox_L4K->value());
    m_DeviceManager->setlarmanDelay(ui->spinBox_larmandelay->value());
    m_DeviceManager->setFrameRate(ui->spinBox_zhenlv->value());
    m_DeviceManager->setExposure(ui->spinBox_baoguang->value());
    m_DeviceManager->setXLines(ui->spinBox_lumo->value());

    diankongConfigs dkc = getDiankongConfigs();
    FileIO::instance()->writeDianKongConfig(dkc);

}

void MainWindow::on_pushButton_turn_clicked()
{
    m_DeviceManager->turnControl(1,ui->spinBox_turn->value());
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


void MainWindow::on_pushButton_pushControl_close_clicked()
{
    m_DeviceManager->pushControl(1,false);
}


void MainWindow::on_pushButton_pushControl_2close_clicked()
{
    m_DeviceManager->pushControl(2,false);
}


void MainWindow::on_pushButton_turn2_clicked()
{
    m_DeviceManager->turnControl(2,ui->spinBox_turn->value());
}


void MainWindow::on_pushButton_saveLog_clicked()
{
    QString text = ui->textEdit->toPlainText();
    QFile file("E:/test/LOG.txt");
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out << text;
        file.close();
    }
}


void MainWindow::on_pushButton_clicked()
{
    m_DeviceManager->lumoCapture(ui->spinBox_lumo->value());
}

