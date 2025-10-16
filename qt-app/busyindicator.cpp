#include "busyindicator.h"
#include <QPainter>
#include <QtMath>

BusyIndicator::BusyIndicator(QWidget *parent)
    : QWidget(parent),
    currentCounter(0),
    numberOfLines(12),
    lineLength(10),
    lineWidth(3),
    color(Qt::white),
    spinning(false)
{
    timer.setInterval(80);
    connect(&timer, &QTimer::timeout, this, &BusyIndicator::rotateStep);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
}

void BusyIndicator::start()
{
    if (!spinning) {
        spinning = true;
        currentCounter = 0;
        timer.start();
        show();
        update();
    }
}

void BusyIndicator::stop()
{
    if (spinning) {
        spinning = false;
        timer.stop();
        hide();
    }
}

bool BusyIndicator::isSpinning() const
{
    return spinning;
}

void BusyIndicator::rotateStep()
{
    currentCounter = (currentCounter + 1) % numberOfLines;
    update();
}

void BusyIndicator::paintEvent(QPaintEvent *)
{
    if (!spinning) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPointF center(width()/2.0, height()/2.0);
    p.translate(center);

    for (int i = 0; i < numberOfLines; ++i) {
        int index = (i + currentCounter) % numberOfLines;
        qreal alpha = (qreal)index / numberOfLines;
        QColor col = color;
        col.setAlphaF(alpha);

        p.save();
        p.rotate((360.0 * i) / numberOfLines);
        QRectF r(lineLength, -lineWidth/2.0, lineLength, lineWidth);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawRoundedRect(r, 1.5, 1.5);
        p.restore();
    }
}
