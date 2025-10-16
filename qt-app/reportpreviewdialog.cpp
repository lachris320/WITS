#include "reportpreviewdialog.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QPushButton>

ReportPreviewDialog::ReportPreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Report Preview");
    resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);
    textBrowser = new QTextBrowser(this);
    QPushButton *closeBtn = new QPushButton("Close", this);

    layout->addWidget(textBrowser);
    layout->addWidget(closeBtn);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ReportPreviewDialog::setHtml(const QString &html) {
    textBrowser->setHtml(html);
}
