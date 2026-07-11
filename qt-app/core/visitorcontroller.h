#ifndef VISITORCONTROLLER_H
#define VISITORCONTROLLER_H
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include "visitordata.h"

class QNetworkAccessManager;

class VisitorController : public QObject
{
    Q_OBJECT
public:
    explicit VisitorController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Async — result arrives via visitorsLoaded / fetchError.
    void fetchVisitors(const VisitorFilter &filter);

    // Synchronous QXlsx export. Returns xlsx.saveAs(filePath).
    bool exportToExcel(const QList<VisitorRecord> &records,
                       const VisitorFilter &filter,
                       const QString &schoolName,
                       const QString &filePath);

    // Pure, unit-testable statics
    static QString wireDateType(DateFilterType t);
    static QPair<QString, QString> monthRange(int month, int year);
    static QString defaultExportFileName(const VisitorFilter &f);
    static bool parseVisitorsResponse(const QByteArray &json,
                                      QList<VisitorRecord> *out,
                                      int *totalCount,
                                      QString *errorMsg);

signals:
    void visitorsLoaded(const QList<VisitorRecord> &records, int totalCount);
    void fetchError(const QString &title, const QString &message);

private:
    // Shared by defaultExportFileName and exportToExcel so the filename and
    // the "Filter Applied" cell can never drift apart.
    static QString datePartForFilter(const VisitorFilter &f);

    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // VISITORCONTROLLER_H
