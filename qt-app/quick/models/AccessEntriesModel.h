#ifndef ACCESSENTRIESMODEL_H
#define ACCESSENTRIESMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include "loginparser.h"

// Access Control recent-entries rows (Sub-plan 4). TEXT ONLY — no photo
// thumbnails this slice, so the Sub-plan 3 photo-origin problem is not
// re-introduced into the admin table. A row whose endpoint student was null
// renders name "Unknown card" with known == false.
class AccessEntriesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        SchoolIdRole,
        CourseRole,
        DepartmentRole,
        CreatedAtRole,
        ReaderRole,
        CardRole,
        KnownRole,
    };

    explicit AccessEntriesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QVector<LoginParser::RecentEntry> &entries);
    void clear();

private:
    QVector<LoginParser::RecentEntry> m_entries;
};

#endif // ACCESSENTRIESMODEL_H
