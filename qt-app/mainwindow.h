#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMainWindow>
#include <QJsonObject>
#include "adminwindow.h"
#include "guestwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void handleLogin();
    void toggleFullscreen();
    void updateSchoolInfo(const QString &schoolName, const QString &address, const QFont &font);

private:
    Ui::MainWindow *ui;
    adminWindow *adminWin;   // Admin dashboard window
    QNetworkAccessManager *networkManager;
    QList<QJsonObject> recentLogins;

    void refreshRightPanel();
    void updateTimeandDate();
    void updateLogo(const QString &logoPath);
    void updatePoster(const QString &posterPath);
    void displayStudent(const QJsonObject &student);
};

#endif // MAINWINDOW_H
