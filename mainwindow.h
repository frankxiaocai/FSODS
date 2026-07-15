#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "./CoreTools/fileio.h"
#include "./CoreTools/mystruct.h"
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
    void initUI();

private slots:
    void on_pushButton_initCamera_clicked();
    void on_pushButton_initLumo_clicked();
    void on_pushButton_initLarman_clicked();
    void on_pushButton_initEleControl_clicked();
    void on_pushButton_LarmanCap_clicked();

    void on_pushButton_pushControl_1_clicked();
    void on_pushButton_pushControl_2_clicked();
    void on_pushButton_pushControl_close_clicked();
    void on_pushButton_pushControl_2close_clicked();
    void on_pushButton_turn_clicked();
    void on_pushButton_turn2_clicked();

    void on_pushButton_beltonoff_clicked();
    void on_pushButton_beltonoff_2_clicked();

    void on_checkBox_clicked(bool checked);
    void on_pushButton_apply_clicked();
    void on_pushButton_saveLog_clicked();

    void on_pushButton_close_clicked();
    void on_pushButton_mini_clicked();
    void on_pushButton_test_clicked();



private:
    Ui::MainWindow *ui;
    DeviceManager* m_DeviceManager;

    void showError(Error_code err);// 错误提示函数
    QString plasticTypeToString(int code);//类型显示
    void updateCountLabel(int type);//计数
    diankongConfigs getDiankongConfigs();
    void setDiankongConfigs(diankongConfigs dk);

};
#endif // MAINWINDOW_H
