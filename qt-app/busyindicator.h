#ifndef BUSYINDICATOR_H
#define BUSYINDICATOR_H

#include <QWidget>
#include <QTimer>
#include <QColor>

class BusyIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit BusyIndicator(QWidget *parent = nullptr);
    void start();
    void stop();
    bool isSpinning() const;

    void setColor(const QColor &c) { color = c; }
    QSize sizeHint() const override { return QSize(48,48); }

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void rotateStep();

private:
    QTimer timer;
    int currentCounter;
    int numberOfLines;
    int lineLength;
    int lineWidth;
    QColor color;
    bool spinning;
};

#endif // BUSYINDICATOR_H
