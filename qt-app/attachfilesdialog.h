#ifndef ATTACHFILESDIALOG_H
#define ATTACHFILESDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>

class AttachFilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AttachFilesDialog(QWidget *parent = nullptr);
    ~AttachFilesDialog();

    QString getExcelPath() const { return m_excelPath; }
    QString getZipPath() const { return m_zipPath; }

private slots:
    void onExcelBtnClicked();
    void onZipBtnClicked();

private:
    QPushButton *excelBtn;
    QPushButton *zipBtn;
    QPushButton *okBtn;
    QPushButton *cancelBtn;
    QLabel *excelLabel;
    QLabel *zipLabel;

    QString m_excelPath;
    QString m_zipPath;
};

#endif // ATTACHFILESDIALOG_H
