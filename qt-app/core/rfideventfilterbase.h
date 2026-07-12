#ifndef RFIDEVENTFILTERBASE_H
#define RFIDEVENTFILTERBASE_H

#include <QObject>
#include <QElapsedTimer>
#include "rfidscandetector.h"

// Shared, widget-free key-burst filter. Feeds printable keys (and a normalized
// terminator) to an RfidScanDetector while gateOpen(watched) is true; on a
// recognized scan it consumes the terminating Enter and emits the code. The
// window-type-specific "is my login window active?" test is the only thing that
// differs between the Widgets and Quick installs, so it is the pure-virtual seam.
class RfidEventFilterBase : public QObject
{
    Q_OBJECT
public:
    explicit RfidEventFilterBase(QObject *parent = nullptr);

    static bool isTerminator(int key);

signals:
    void rfidScanned(const QString &code);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    // True when a key delivered to `watched` should be fed to the detector.
    // Subclasses encode their window-active (and, for Widgets, ancestor-dedup)
    // rules here.
    virtual bool gateOpen(QObject *watched) const = 0;

private:
    RfidScanDetector m_detector;
    QElapsedTimer m_clock;
};

#endif // RFIDEVENTFILTERBASE_H
