#include "rfidreader.h"

RFIDReader::RFIDReader(QObject *parent)
    : QObject(parent)
    , serialPort(new QSerialPort(this))
{
    connect(serialPort, &QSerialPort::readyRead, this, &RFIDReader::readData);
    connect(serialPort, &QSerialPort::errorOccurred, this, &RFIDReader::handleError);
}

RFIDReader::~RFIDReader()
{
    if (serialPort->isOpen()) {
        serialPort->close();
    }
}

bool RFIDReader::connectToReader(const QString &portName)
{
    if (serialPort->isOpen()) {
        serialPort->close();
    }

    if (portName.isEmpty()) {
        // Auto-detect RFID reader
        return findAndConnectReader();
    }

    serialPort->setPortName(portName);
    serialPort->setBaudRate(QSerialPort::Baud9600);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (serialPort->open(QIODevice::ReadOnly)) {
        emit connectionStatusChanged(true);
        return true;
    } else {
        // Silent failure - just log without error signal
        emit connectionStatusChanged(false);
        return false;
    }
}

bool RFIDReader::findAndConnectReader()
{
    // Try to find RFID reader automatically
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &portInfo : ports) {
        // Common RFID reader identifiers
        QString desc = portInfo.description().toLower();
        QString manuf = portInfo.manufacturer().toLower();

        if (desc.contains("usb") || desc.contains("serial") ||
            manuf.contains("prolific") || manuf.contains("ftdi") ||
            desc.contains("ch340") || desc.contains("cp210")) {

            if (connectToReader(portInfo.portName())) {
                return true;
            }
        }
    }

    // Don't emit error - just log it (allows silent failure when RFID is optional)
    return false;
}

void RFIDReader::disconnect()
{
    if (serialPort->isOpen()) {
        serialPort->close();
        emit connectionStatusChanged(false);
    }
}

bool RFIDReader::isConnected() const
{
    return serialPort->isOpen();
}

QStringList RFIDReader::availablePorts() const
{
    QStringList portNames;
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &portInfo : ports) {
        portNames << portInfo.portName();
    }

    return portNames;
}

void RFIDReader::readData()
{
    QByteArray data = serialPort->readAll();
    buffer.append(QString::fromLatin1(data));

    // Protect against buffer overflow from malformed data
    if (buffer.size() > 1024) {  // Reasonable limit for RFID data (typically <50 chars)
        buffer.clear();
        emit errorOccurred("Buffer overflow - received too much data without newline");
        return;
    }

    // Most RFID readers send data terminated with \r\n or \n
    if (buffer.contains('\n') || buffer.contains('\r')) {
        QString rfidId = buffer.trimmed();
        buffer.clear();

        if (!rfidId.isEmpty()) {
            emit rfidDetected(rfidId);
        }
    }
}

void RFIDReader::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        serialPort->close();
        emit connectionStatusChanged(false);
        emit errorOccurred("RFID reader disconnected");
    } else if (error != QSerialPort::NoError) {
        emit errorOccurred(serialPort->errorString());
    }
}
