#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "./CoreTools/logger.h"
#include "devicemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void init();

private slots:
    void on_pushButton_initCamera_clicked();
    void on_pushButton_initLumo_clicked();
    void on_pushButton_lumoCapture_clicked();
    void on_pushButton_initLarman_clicked();
    void on_pushButton_LarmanCap_clicked();
    void on_pushButton_initEleControl_clicked();
    void on_pushButton_close_clicked();
    void on_pushButton_mini_clicked();
    void on_pushButton_test_clicked();
    void on_pushButton_pushControl_1_clicked();
    void on_pushButton_pushControl_2_clicked();

    void slot_autoCapture();

    void on_pushButton_lumoCapture_auto_clicked();

private:
    Ui::MainWindow *ui;
    DeviceManager* m_DeviceManager;
    QTimer* m_autoTimer;

    void showError(Error_code err);// 错误提示函数
    QString plasticTypeToString(int code);
};
#endif // MAINWINDOW_H
