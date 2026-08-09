#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainViewModel;
class SimulatedModbusServer;
class SimulatedSerialServer;
class CurvePanel;
class QTreeWidget;
class QTreeWidgetItem;
class QTableView;
class QTableWidget;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// 自动启动模拟下位机（命令行 --sim 时调用，便于演示/自动化测试）
    bool startSimulatorAutomatically();

    /// 从 JSON 加载设备与通道配置
    void loadConfiguration(const QString &jsonPath);

    /// 触发连接下位机（命令行 --sim 自动测试用）
    void connectToDevice();

    /// 自检模式：启动模拟器+连接+采集，把通道值写入 outPath 后退出
    void runSelfTest(const QString &outPath);

    /// 重连自检：停掉 1503（电机PLC-2）→ 断言离线 → 重启 → 断言自动重连恢复
    void runSelfTestReconnect(const QString &outPath);

private slots:
    /// 启动 / 停止内置模拟下位机（Modbus TCP Server @ 127.0.0.1:1502）
    void toggleSimulator();

private:
    void setupUi();
    void bindToViewModel();
    void createStatusBar();
    void appendAlarmRow(const QString &time, const QString &severity,
                        const QString &device, const QString &message);
    /// Refresh all device-tree rows from the collector's live state.
    void updateDeviceStatus();

    MainViewModel         *m_viewModel = nullptr;
    SimulatedModbusServer *m_simulator  = nullptr;   // TCP 设备1 @ 1502
    SimulatedModbusServer *m_simulator2 = nullptr;   // TCP 设备2 @ 1503
    SimulatedSerialServer *m_serialSim  = nullptr;   // 串口设备 @ COM6

    // --- UI widgets (kept as members for binding) ---
    QTreeWidget        *m_deviceTree  = nullptr;   // 硬件列表
    QList<QTreeWidgetItem*> m_deviceItems;         // 设备树行
    QTableView         *m_dataTable   = nullptr;   // 通道数据
    QLabel             *m_tableTitle  = nullptr;   // 表格标题（含当前设备名）
    CurvePanel         *m_curvePanel  = nullptr;   // 实时曲线
    QTableWidget       *m_alarmTable  = nullptr;   // 报警列表
    QLabel             *m_statusLabel = nullptr;   // 连接状态
};

#endif // MAINWINDOW_H
