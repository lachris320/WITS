#include "AccessEntriesModel.h"

AccessEntriesModel::AccessEntriesModel(QObject *parent) : QAbstractListModel(parent) {}

int AccessEntriesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant AccessEntriesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const LoginParser::RecentEntry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole:       return e.known ? e.name : tr("Unknown card");
    case SchoolIdRole:   return e.schoolId;
    case CourseRole:     return e.course;
    case DepartmentRole: return e.department;
    case CreatedAtRole:  return e.createdAt;
    case ReaderRole:     return QString::number(e.reader);
    case CardRole:       return e.card;
    case KnownRole:      return e.known;
    default:             return {};
    }
}

QHash<int, QByteArray> AccessEntriesModel::roleNames() const
{
    return {
        { NameRole, "name" }, { SchoolIdRole, "schoolId" }, { CourseRole, "course" },
        { DepartmentRole, "department" }, { CreatedAtRole, "createdAt" },
        { ReaderRole, "reader" }, { CardRole, "card" }, { KnownRole, "known" },
    };
}

void AccessEntriesModel::setEntries(const QVector<LoginParser::RecentEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void AccessEntriesModel::clear()
{
    setEntries({});
}
