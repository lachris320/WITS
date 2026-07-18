#ifndef SEARCHRESULTSMODEL_H
#define SEARCHRESULTSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "studentdata.h"

// Search results row model (spec §6.2). The `visits` role is rendered by the
// result card as the explicit "Total Visits: N" label (spec §5.3 layer 4).
class SearchResultsModel : public QAbstractListModel
{
    Q_OBJECT
    // A QAbstractListModel has no `count` property; SearchScreen reads
    // `vm.results.count` from QML, so expose one explicitly (a raw model would
    // read `undefined` at runtime and only "work" against a QML ListModel stub).
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        SchoolIdRole,
        CourseRole,
        DepartmentRole,
        YearLevelRole,
        StatusRole,
        VisitsRole,
        InitialsRole,
    };

    explicit SearchResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_records.size(); }
    void setRecords(const QList<StudentRecord> &records);

signals:
    void countChanged();

private:
    QList<StudentRecord> m_records;
};

#endif // SEARCHRESULTSMODEL_H
