#ifndef IMPORTDATA_H
#define IMPORTDATA_H
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

enum class ImportSeverity { Warning, Critical };

// Distinguishes the three legacy Excel-load failure dialogs so the View can
// show the exact original message + severity. None = a usable table was read.
enum class ExcelParseError { None, OpenFailed, NoSheets, EmptySheet };

struct ParsedTable
{
    QStringList headers;
    QList<QStringList> rows;
    QMap<QString, int> headerIndex;
};

struct UploadResult
{
    bool    ok = false;
    QString message;
    int     successCount = 0;
    int     errorCount = 0;
    bool    plainText = false;   // true when the response was not a JSON object
    QString rawText;             // populated only when plainText is true
};

#endif // IMPORTDATA_H
