#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainViewModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    void bindToViewModel();
    void createStatusBar();

    MainViewModel *m_viewModel = nullptr;
};

#endif // MAINWINDOW_H
