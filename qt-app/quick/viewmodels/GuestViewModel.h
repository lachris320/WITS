#ifndef GUESTVIEWMODEL_H
#define GUESTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <qqml.h>

class QNetworkAccessManager;
class HttpClient;

// Guest sign-in: validates + POSTs guest_login.php. Mirrors guestwindow.cpp.
class GuestViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit GuestViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);

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
    HttpClient *m_http = nullptr;              // injected, or an owned child
    QNetworkAccessManager *m_nam = nullptr;    // m_http->manager(); non-owning
};

#endif // GUESTVIEWMODEL_H
