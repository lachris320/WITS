#include "rfidscandetector.h"

RfidScanDetector::RfidScanDetector(qint64 maxInterKeyGapMs, int minCodeLength, int maxBufferLength)
    : m_lastTimestampMs(-1)
    , m_maxInterKeyGapMs(maxInterKeyGapMs)
    , m_minCodeLength(minCodeLength)
    , m_maxBufferLength(maxBufferLength)
{
}

std::optional<QString> RfidScanDetector::feedKey(QChar character, qint64 timestampMs)
{
    if (character == QChar('\n') || character == QChar('\r')) { // terminator
        std::optional<QString> result;
        if (m_buffer.length() >= m_minCodeLength)
            result = m_buffer;
        reset();
        return result;
    }

    m_buffer.append(character);
    m_lastTimestampMs = timestampMs;
    return std::nullopt;
}

void RfidScanDetector::reset()
{
    m_buffer.clear();
    m_lastTimestampMs = -1;
}
