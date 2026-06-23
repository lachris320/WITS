#ifndef RFIDREADER_H
#define RFIDREADER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QString>
#include <QDebug>

class RFIDReader : public QObject
{
    Q_OBJECT

public:
    explicit RFIDReader(QObject *parent = nullptr);
    ~RFIDReader();

    bool connectToReader(const QString &portName = "");
    void disconnect();
    bool isConnected() const;
    QStringList availablePorts() const;

signals:
    void rfidDetected(const QString &rfidId);
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &error);

private slots:
    void readData();
    void handleError(QSerialPort::SerialPortError error);

private:
    QSerialPort *serialPort;
    QString buffer;
    bool findAndConnectReader();
};

#endif // RFIDREADER_H
