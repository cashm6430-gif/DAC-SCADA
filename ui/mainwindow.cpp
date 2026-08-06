#include "mainwindow.h"
#include "main_viewmodel.h"

#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTableView>
#include <QLabel>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDateTime>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_viewModel(new MainViewModel(this))
{
    setupUi();
    bindToViewModel();
    createStatusBar();

    setWindowTitle("DAC-SCADA");
    resize(1280, 720);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// UI setup — pure widget creation, no business logic
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    // --- menu bar ---
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Connect"),    this, [this]() { m_viewModel->connectToDevice(); });
    fileMenu->addAction(tr("&Disconnect"), this, [this]() { m_viewModel->disconnectFromDevice(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("&Refresh Devices"), this, [this]() { m_viewModel->scanDevices(); });

    // --- toolbar ---
    QToolBar *toolbar = addToolBar(tr("Main"));
    toolbar->addAction(tr("Connect"),    this, [this]() { m_viewModel->connectToDevice(); });
    toolbar->addAction(tr("Disconnect"), this, [this]() { m_viewModel->disconnectFromDevice(); });
    toolbar->addSeparator();
    toolbar->addAction(tr("Refresh"),    this, [this]() { m_viewModel->scanDevices(); });

    // --- central widget: data table ---
    auto *central = new QWidget(this);
    auto *vbox    = new QVBoxLayout(central);

    auto *tableLabel = new QLabel(tr("Real-time Data"), central);
    tableLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    vbox->addWidget(tableLabel);

    auto *table = new QTableView(central);
    table->setModel(m_viewModel->dataModel());
    vbox->addWidget(table);

    setCentralWidget(central);

    // --- device dock ---
    auto *deviceDock = new QDockWidget(tr("Devices"), this);
    auto *deviceTree = new QTreeWidget(deviceDock);
    deviceTree->setHeaderLabel(tr("Device Tree"));
    deviceDock->setWidget(deviceTree);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock);

    // --- alarm dock ---
    auto *alarmDock = new QDockWidget(tr("Alarms"), this);
    auto *alarmTree = new QTreeWidget(alarmDock);
    alarmTree->setHeaderLabels({tr("Time"), tr("Severity"), tr("Message")});
    alarmDock->setWidget(alarmTree);
    addDockWidget(Qt::BottomDockWidgetArea, alarmDock);
}

void MainWindow::createStatusBar()
{
    auto *statusLabel = new QLabel(tr("Disconnected"));
    statusLabel->setObjectName("connectionStatus");
    statusBar()->addPermanentWidget(statusLabel);
}

// ---------------------------------------------------------------------------
// ViewModel binding — all View ↔ ViewModel connections
// ---------------------------------------------------------------------------

void MainWindow::bindToViewModel()
{
    // ---- connected state → status bar ----
    connect(m_viewModel, &MainViewModel::connectedChanged, this, [this]() {
        auto *label = statusBar()->findChild<QLabel *>("connectionStatus");
        if (label)
            label->setText(m_viewModel->isConnected()
                ? tr("Connected") : tr("Disconnected"));
    });

    // ---- status text → status bar message ----
    connect(m_viewModel, &MainViewModel::statusTextChanged, this, [this](const QString &text) {
        statusBar()->showMessage(text, 3000);
    });

    // ---- connection errors → dialog ----
    connect(m_viewModel, &MainViewModel::connectionError, this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Connection Error"), msg);
    });

    // ---- new alarm → alarm dock ----
    connect(m_viewModel, &MainViewModel::newAlarmTriggered, this, [this](const QString &msg) {
        // Find the alarm dock tree widget and add a row
        for (auto *dock : findChildren<QDockWidget *>()) {
            if (dock->windowTitle() == tr("Alarms")) {
                if (auto *tree = dock->findChild<QTreeWidget *>()) {
                    auto *item = new QTreeWidgetItem(tree);
                    item->setText(0, QDateTime::currentDateTime().toString("hh:mm:ss"));
                    item->setText(1, tr("Warning"));
                    item->setText(2, msg);
                }
                break;
            }
        }
    });

    // ---- device list changed → device tree ----
    connect(m_viewModel, &MainViewModel::deviceListChanged, this, [this]() {
        // Find the device dock tree and refresh
        for (auto *dock : findChildren<QDockWidget *>()) {
            if (dock->windowTitle() == tr("Devices")) {
                if (auto *tree = dock->findChild<QTreeWidget *>()) {
                    tree->clear();
                    // We don't have direct access to the device list through Q_PROPERTY yet,
                    // but the connection keeps the two in sync for future expansion.
                    Q_UNUSED(tree)
                }
                break;
            }
        }
    });
}
