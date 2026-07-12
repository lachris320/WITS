#ifndef GUESTVIEWMODEL_H
#define GUESTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <qqml.h>

class QNetworkAccessManager;

// Guest sign-in: validates + POSTs guest_login.php. Mirrors guestwindow.cpp.
class GuestViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit GuestViewModel(QObject *parent = nullptr);

    Q_INVOKABLE void submitGuest(const QString &name, const QString &contact,
                                 const QString &company, const QString &purpose);

    static bool validate(const QString &name, const QString &company,
                         const QString &purpose);

    // Network-free seam for tests + the reply handler.
    void applyGuestResponse(const QByteArray &json);

signals:
    void guestSucceeded(const QString &message);
    void guestFailed(const QString &message);

private:
    QNetworkAccessManager *m_nam = nullptr;
};

#endif // GUESTVIEWMODEL_H
