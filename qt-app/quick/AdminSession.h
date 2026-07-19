#ifndef ADMINSESSION_H
#define ADMINSESSION_H

#include <QObject>
#include <QString>

// In-memory holder for the admin key captured at the kiosk admin-login gate
// (spec §3.3). RAM ONLY — NEVER written to QSettings. A process-wide singleton
// so the capture site (KioskViewModel) and the consumers (SettingsViewModel,
// later DatabaseViewModel) share ONE instance with no QML plumbing. Not
// QML-registered: no Phase-4c QML consumer needs it directly (YAGNI).
class AdminSession : public QObject
{
    Q_OBJECT
public:
    static AdminSession &instance();

    QString key() const { return m_key; }
    bool hasKey() const { return !m_key.isEmpty(); }

    // Capture at the admin-entry gate.
    void setKey(const QString &key);
    // Same-session key change in Settings — refresh so subsequent destructive
    // ops don't 401 on a stale held key (spec §3.3).
    void refresh(const QString &newKey) { setKey(newKey); }
    void clear();

signals:
    void changed();

private:
    explicit AdminSession(QObject *parent = nullptr) : QObject(parent) {}
    Q_DISABLE_COPY(AdminSession)
    QString m_key;
};

#endif // ADMINSESSION_H
