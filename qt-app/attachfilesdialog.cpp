#include "attachfilesdialog.h"
#include "theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

AttachFilesDialog::AttachFilesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Attach Files");
    setMinimumWidth(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Excel file section
    QLabel *excelTitle = new QLabel("Excel/CSV File:", this);
    excelLabel = new QLabel("No file selected", this);
    excelLabel->setStyleSheet(QStringLiteral("color: %1; font-style: italic;").arg(WitsTheme::Color::MutedText));
    excelBtn = new QPushButton("Browse Excel/CSV...", this);

    mainLayout->addWidget(excelTitle);
    mainLayout->addWidget(excelLabel);
    mainLayout->addWidget(excelBtn);
    mainLayout->addSpacing(20);

    // ZIP file section
    QLabel *zipTitle = new QLabel("Photos ZIP File (Optional):", this);
    zipLabel = new QLabel("No file selected", this);
    zipLabel->setStyleSheet(QStringLiteral("color: %1; font-style: italic;").arg(WitsTheme::Color::MutedText));
    zipBtn = new QPushButton("Browse ZIP...", this);

    mainLayout->addWidget(zipTitle);
    mainLayout->addWidget(zipLabel);
    mainLayout->addWidget(zipBtn);
    mainLayout->addSpacing(20);

    // OK/Cancel buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    okBtn = new QPushButton("OK", this);
    cancelBtn = new QPushButton("Cancel", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(excelBtn, &QPushButton::clicked, this, &AttachFilesDialog::onExcelBtnClicked);
    connect(zipBtn, &QPushButton::clicked, this, &AttachFilesDialog::onZipBtnClicked);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

AttachFilesDialog::~AttachFilesDialog()
{
    // No UI pointer to delete since we're not using .ui file
}

void AttachFilesDialog::onExcelBtnClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select Excel/CSV File",
                                                    "", "Excel Files (*.xlsx *.xls *.csv)");
    if (!filePath.isEmpty()) {
        m_excelPath = filePath;
        excelLabel->setText(QFileInfo(filePath).fileName());
        excelLabel->setStyleSheet(QStringLiteral("color: %1; font-style: normal;").arg(WitsTheme::Color::Text));
    }
}

void AttachFilesDialog::onZipBtnClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select ZIP File",
                                                    "", "ZIP Files (*.zip)");
    if (!filePath.isEmpty()) {
        m_zipPath = filePath;
        zipLabel->setText(QFileInfo(filePath).fileName());
        zipLabel->setStyleSheet(QStringLiteral("color: %1; font-style: normal;").arg(WitsTheme::Color::Text));
    }
}
