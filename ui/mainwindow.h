#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainViewModel;
class SimulatedModbusServer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// 自动启动模拟下位机（命令行 --sim 时调用，便于演示/自动化测试）
    bool startSimulatorAutomatically();

private slots:
    /// 启动 / 停止内置模拟下位机（Modbus TCP Server @ 127.0.0.1:1502）
    void toggleSimulator();

private:
    void setupUi();
    void bindToViewModel();
    void createStatusBar();

    MainViewModel         *m_viewModel = nullptr;
    SimulatedModbusServer *m_simulator = nullptr;
};

#endif // MAINWINDOW_H
