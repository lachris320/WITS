#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMainWindow>
#include <QElapsedTimer>
#include <QJsonObject>
#include "adminwindow.h"
#include "guestwindow.h"

class RfidKeyboardFilter;
class QLabel;

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
    void handleRfidLogin(const QString &code);

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
    void showKioskStatus(const QString &message, bool error);

    RfidKeyboardFilter *rfidFilter = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_posterBg = nullptr;
    void syncPosterBg();   // size the layer to frame_2 and re-render the scrimmed poster
    QString m_posterPath;  // remembered so resize can re-render at the new size
    QElapsedTimer m_rfidDebounceClock;
    QString m_lastRfidCode;
    qint64 m_lastRfidMs = -100000;
};

#endif // MAINWINDOW_H
