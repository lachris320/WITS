#ifndef REPORTPREVIEWDIALOG_H
#define REPORTPREVIEWDIALOG_H

#include <QDialog>

class QTextBrowser;

class ReportPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReportPreviewDialog(QWidget *parent = nullptr);

    void setHtml(const QString &html);

private:
    QTextBrowser *textBrowser;
};

#endif // REPORTPREVIEWDIALOG_H
