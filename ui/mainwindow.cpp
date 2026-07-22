#include "mainwindow.h"
#include "core/device_manager.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"
#include "communication/serialport_comm.h"
#include "communication/modbus_rtu.h"

#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QMessageBox>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // --- core objects ---
    m_serial    = new SerialPortComm(this);
    m_modbus    = new ModbusRtu(m_serial, this);
    m_deviceMgr = new DeviceManager(m_modbus, this);
    m_cache     = new DataCache(this);
    m_alarms    = new AlarmEngine(this);

    setupUi();
    setupConnections();
    createStatusBar();
    setWindowTitle("DAC-SCADA");
    resize(1280, 720);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    // --- menu bar ---
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Connect"),    this, &MainWindow::onConnectClicked);
    fileMenu->addAction(tr("&Disconnect"), this, &MainWindow::onDisconnectClicked);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("&Refresh Devices"), this, &MainWindow::onRefreshDevices);

    // --- toolbar ---
    QToolBar *toolbar = addToolBar(tr("Main"));
    toolbar->addAction(tr("Connect"),    this, &MainWindow::onConnectClicked);
    toolbar->addAction(tr("Disconnect"), this, &MainWindow::onDisconnectClicked);
    toolbar->addSeparator();
    toolbar->addAction(tr("Refresh"), this, &MainWindow::onRefreshDevices);

    // --- central widget: data table ---
    auto *central = new QWidget(this);
    auto *vbox    = new QVBoxLayout(central);

    auto *tableLabel = new QLabel(tr("Real-time Data"), central);
    tableLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    vbox->addWidget(tableLabel);

    auto *table = new QTableView(central);
    table->setModel(m_cache->tableModel());
    vbox->addWidget(table);

    setCentralWidget(central);

    // --- device dock ---
    auto *deviceDock = new QDockWidget(tr("Devices"), this);
    auto *tree       = new QTreeWidget(deviceDock);
    tree->setHeaderLabel(tr("Device Tree"));
    deviceDock->setWidget(tree);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock);

    // --- alarm dock ---
    auto *alarmDock = new QDockWidget(tr("Alarms"), this);
    auto *alarmList = new QTreeWidget(alarmDock);
    alarmList->setHeaderLabels({tr("Time"), tr("Severity"), tr("Message")});
    alarmDock->setWidget(alarmList);
    addDockWidget(Qt::BottomDockWidgetArea, alarmDock);

    connect(tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                if (current)
                    onDeviceSelected(current->data(0, Qt::UserRole).toInt());
            });
}

void MainWindow::setupConnections()
{
    // serial state → UI
    connect(m_serial, &CommInterface::stateChanged,
            this, &MainWindow::updateConnectionState);

    // device data → cache
    connect(m_deviceMgr, &DeviceManager::deviceDataChanged,
            m_cache, &DataCache::updateValue);

    // alarms
    connect(m_deviceMgr, &DeviceManager::alarmTriggered,
            m_alarms, &AlarmEngine::onAlarmTriggered);
}

void MainWindow::createStatusBar()
{
    auto *statusLabel = new QLabel(tr("Disconnected"));
    statusLabel->setObjectName("connectionStatus");
    statusBar()->addPermanentWidget(statusLabel);
}

// ---------------------------------------------------------------------------
// slots
// ---------------------------------------------------------------------------

void MainWindow::onConnectClicked()
{
    if (!m_serial || m_serial->isOpen()) return;

    // In a real app you'd get these from a settings dialog or config.
    m_serial->setPortName("COM3");
    m_serial->setBaudRate(9600);

    if (!m_serial->open()) {
        QMessageBox::warning(this, tr("Connection Error"),
                             tr("Failed to open serial port."));
        return;
    }
    m_modbus->open();
}

void MainWindow::onDisconnectClicked()
{
    m_modbus->close();
    m_serial->close();
}

void MainWindow::onRefreshDevices()
{
    m_deviceMgr->scan();
}

void MainWindow::onDeviceSelected(int index)
{
    m_deviceMgr->setActiveDevice(index);
}

void MainWindow::updateConnectionState(bool connected)
{
    auto *label = statusBar()->findChild<QLabel *>("connectionStatus");
    if (label)
        label->setText(connected ? tr("Connected") : tr("Disconnected"));
}
