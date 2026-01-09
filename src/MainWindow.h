#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void createDummyPly(const QString& filename);

private slots:
    void onOpenActionTriggered(); // 파일 열기 슬롯

private:
    class SplattingWidget *m_splatWidget; // 전방 선언 사용
};

#endif // MAINWINDOW_H
