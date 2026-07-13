#include "VisitLogRowsModel.h"

VisitLogRowsModel::VisitLogRowsModel(QObject *parent) : QAbstractListModel(parent) {}

int VisitLogRowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant VisitLogRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case DateRole:       return r.date;
    case NameRole:       return r.name;
    case CourseRole:     return r.course;
    case DepartmentRole: return r.department;
    case TimeInRole:     return r.timeIn;
    case TimeOutRole:    return r.timeOut;
    case CompanyRole:    return r.company;
    case PurposeRole:    return r.purpose;
    default:             return {};
    }
}

QHash<int, QByteArray> VisitLogRowsModel::roleNames() const
{
    return {
        { DateRole, "date" }, { NameRole, "name" }, { CourseRole, "course" },
        { DepartmentRole, "department" }, { TimeInRole, "timeIn" },
        { TimeOutRole, "timeOut" }, { CompanyRole, "company" }, { PurposeRole, "purpose" },
    };
}

void VisitLogRowsModel::setRows(const QList<Row> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}
