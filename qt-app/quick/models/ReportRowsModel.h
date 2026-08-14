#ifndef REPORTROWSMODEL_H
#define REPORTROWSMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

// visits arrives from get_report_data.php as a COUNT(...) which mysqli
// fetch_assoc returns as a JSON STRING ("5"), not a number. Parse both shapes.
int reportVisits(const QJsonObject &row);

// Per-student rows for the Reporting preview table (spec §4.2). Mirrors the
// StudentsTableModel/BarsModel precedent: roles enum, roleNames(), typed row.
class ReportRowsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, CourseRole, YearLevelRole, VisitsRole };
    explicit ReportRowsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_rows.size(); }
    void setRows(const QJsonArray &data);

signals:
    void countChanged();

private:
    struct Row { QString name; QString course; QString year; int visits = 0; };
    QList<Row> m_rows;
};

#endif // REPORTROWSMODEL_H
