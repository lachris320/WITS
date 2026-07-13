#include "SearchResultsModel.h"

SearchResultsModel::SearchResultsModel(QObject *parent) : QAbstractListModel(parent) {}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant SearchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return {};
    const StudentRecord &r = m_records.at(index.row());
    switch (role) {
    case NameRole:       return r.name;
    case SchoolIdRole:   return r.schoolId;
    case CourseRole:     return r.course;
    case DepartmentRole: return r.department;
    case YearLevelRole:  return r.yearLevel;
    case StatusRole:     return r.status;
    case VisitsRole:     return r.visits;
    default:             return {};
    }
}

QHash<int, QByteArray> SearchResultsModel::roleNames() const
{
    return {
        { NameRole, "name" }, { SchoolIdRole, "schoolId" }, { CourseRole, "course" },
        { DepartmentRole, "department" }, { YearLevelRole, "yearLevel" },
        { StatusRole, "status" }, { VisitsRole, "visits" },
    };
}

void SearchResultsModel::setRecords(const QList<StudentRecord> &records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
    emit countChanged();
}
