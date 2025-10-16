#ifndef ADMINWINDOW_H
#define ADMINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QFont>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QSettings>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QColor>
#include <QGridLayout>
#include <QTimer>
#include <QTableWidget>
#include <QLabel>
#include <QCheckBox>
#include <QHttpMultiPart>
#include <QHttpPart>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "busyindicator.h"
#include "attachfilesdialog.h"
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

QT_BEGIN_NAMESPACE
namespace Ui { class adminWindow; }
QT_END_NAMESPACE

struct ReportPalette {
    QColor headerBg;
    QColor headerText;
    QColor rowEvenBg;
    QColor rowOddBg;
    QColor rowText;

    QVector<QColor> chartColors;
};

class adminWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit adminWindow(QWidget *parent = nullptr);
    ~adminWindow();

signals:
    void schoolInfoUpdated(const QString &schoolName, const QString &address, const QFont &font);
    void logoChanged(const QString &logoPath);
    void posterChanged(const QString &posterPath);
    void clearAttendanceRequested();
    void guestLoginToggled(bool enabled);
    void reportFiltersReady(QJsonObject filters);

private:
    Ui::adminWindow *ui;
    QSet<int> editedRows;
    QNetworkAccessManager *networkManager;
    QGridLayout *chartsPreviewBoxLayout;
    QString selectedPhotoPath;
    QString selectedExcelPath;
    QString selectedZipPath;

    bool changesMade = false;
    bool reportEdited = false;
    void loadSettings(); // Helper to load saved settings into UI
    void saveSettings();
    bool isSettingUpEditMode = false;
    bool cancelUpload = false;
    void populateFilters();
    ReportPalette getPalette(const QString &choice);
    void closeEvent(QCloseEvent *event);
    void on_chartsBtn_clicked();
    void on_selectAllBtn_clicked();
    void updatePalettePreview(const QString &choice);
    void fetchPreviewData(const QJsonObject &filters);
    void connectFilterSignals();
    void performStudentSearch(bool showOverlay=true);
    void displaySearchResults(const QJsonArray &students, const QString &highlightTerm);
    void clearCheckboxes();
    void setupStudentSearchPage();
    void loadSearchFilters();
    void loadAllStudents();
    void onDepartmentFilterChanged(int);
    void onSearchTextChanged(const QString &text);
    void onStudentCheckboxChanged();
    void openViewDialog(const QJsonObject &student);
    void openEditDialog(const QJsonObject &student);
    void saveStudentChanges(const QJsonObject &student);
    void deleteStudents(const QStringList &schoolIds);
    void fetchStudentVisitHistory(const QString &schoolId, QTableWidget *table);
    void bulkUpdateStudents(const QJsonArray &updates);
    void on_extractVisitorBtn_clicked();
    void onStudentTableItemChanged(QTableWidgetItem *item);
    void showTemporaryOverlay(QWidget *parent, const QString &message);
    QMap<QString,int> bulkHeaderIndex;    // track cancellation

    BusyIndicator *searchSpinner = nullptr;
    QLabel *overlayText = nullptr;            // optional
    QGraphicsOpacityEffect *overlayEffect = nullptr;
    void showSearchOverlay();
    void hideSearchOverlay();

    QImage renderChartToImage(QChart *chart, const QSize &targetSize);
    void expandChartPlotArea(QChart *chart, const QSize &size);
    QJsonArray currentReportData;
    QList<QPixmap> chartPixmaps;
    QJsonObject collectReportFilters(bool validateFilters = true);
    void updateChartsPreview(const QJsonArray &data);
    void optimizeChartForPreview(QChart *chart);
    // Add this in the private slots or private section of adminwindow.h
    QJsonObject collectReportFiltersForPreview();
    QImage makeBarChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette);
    QImage makePieChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette);
    QImage makeLineChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette);
    void loadVisitorLogs(const QString &search, const QString &dateType, const QString &startDate, const QString &endDate);

private slots:
    void onAttachFileBtnClicked();
    void onUpdateDatabaseBtnClicked();
    void onEditStudentBtnClicked();
    void onCancelEditBtnClicked();
    void onSearchBtnClicked();
    void onBrowsePhotoBtnClicked();
    void onDeleteStudentBtnClicked();
    void loadCSVtoTable(const QString &filePath);
    void loadExcelToTable(const QString &filePath);
    void setActiveSidebar(QPushButton* activeBtn);
    void onDurationChanged(int index);
    void updatePreviewLabel();
    void onApplyChangesBtnClicked();
    void onSchoolLogoBrowseBtnClicked();
    void onPosterBrowseBtnClicked();
    void onClearAttendanceCheckBoxStateChanged(int state);
    void onCancelUploadBtnClicked();
    void loadDepartments();
    void loadFilterDepartments();
    void onGeneratePDFBtnClicked();
    void onGenerateExcelBtnClicked();
    void onFilterDepartmentBoxCurrentIndexChanged(int index);
    void fetchReportData(const QJsonObject &filters);
    void exportReportToPDF(const QJsonArray &data, const QJsonObject &filters);
    void exportReportToExcel(const QJsonArray &data, const QJsonObject &filters);
    void loadAvailableYears();
};


#endif // ADMINWINDOW_H
