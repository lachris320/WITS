#ifndef RFIDSCANDETECTOR_H
#define RFIDSCANDETECTOR_H

#include <optional>
#include <QString>
#include <QChar>

// Pure, widget-free state machine that recognizes a fast, terminator-ended
// keystroke burst (an HID RFID scan) and distinguishes it from human typing.
// Fed (character, timestampMs) pairs by RfidKeyboardFilter; the terminator
// arrives as the character '\n' or '\r' (never a Qt keycode).
class RfidScanDetector
{
public:
    explicit RfidScanDetector(qint64 maxInterKeyGapMs = 60,
                              int minCodeLength = 3,
                              int maxBufferLength = 64);

    // Returns the completed code on a recognized scan; std::nullopt otherwise.
    std::optional<QString> feedKey(QChar character, qint64 timestampMs);
    void reset();

private:
    QString m_buffer;
    qint64 m_lastTimestampMs;
    const qint64 m_maxInterKeyGapMs;
    const int m_minCodeLength;
    const int m_maxBufferLength;
};

#endif // RFIDSCANDETECTOR_H
