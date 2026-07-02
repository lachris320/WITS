#ifndef IMPORTCONTROLLER_H
#define IMPORTCONTROLLER_H
#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include "importdata.h"

class QNetworkAccessManager;

class ImportController : public QObject
{
    Q_OBJECT
public:
    explicit ImportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static QString normalizeHeader(const QString &raw);
    static void    mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut);
    static ParsedTable parseCsv(const QString &rawText);

    // Synchronous QXlsx parse. Requires a QGuiApplication (see Testing).
    // errorOut (when non-null) reports which of the three legacy failure
    // cases occurred so the View can show the exact original dialog.
    ParsedTable parseExcel(const QString &filePath, ExcelParseError *errorOut = nullptr);

    // Async — result arrives via duplicatesResolved / importError.
    void checkDuplicates(const QStringList &schoolIds);

    // Async — result arrives via uploadStarted / uploadProgress / uploadFinished / uploadFailed.
    void uploadStudents(const QString &excelPath, const QString &zipPath,
                        const QStringList &skipIds);

    // Pure response parsers
    static bool parseDuplicateResponse(const QByteArray &raw,
                                       QStringList *duplicatesOut,
                                       QString *errorOut);
    static UploadResult parseUploadResponse(const QByteArray &raw);

signals:
    void duplicatesResolved(const QStringList &duplicates);   // empty = none found
    void importError(const QString &title, const QString &message, ImportSeverity severity);
    void uploadStarted();                        // excel file opened OK; request about to post
    void uploadProgress(int percent);
    void uploadFinished(const UploadResult &result);
    void uploadFailed(const QString &message);   // always critical, title "Upload Failed"

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // IMPORTCONTROLLER_H
