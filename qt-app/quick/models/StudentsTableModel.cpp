#include "StudentsTableModel.h"

StudentsTableModel::StudentsTableModel(QObject *parent)
    : QAbstractListModel(parent) {}

int StudentsTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant StudentsTableModel::data(const QModelIndex &index, int role) const
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
    case SelectedRole:   return m_selected.contains(r.schoolId);
    default:             return {};
    }
}

QHash<int, QByteArray> StudentsTableModel::roleNames() const
{
    return {
        {NameRole,"name"}, {SchoolIdRole,"schoolId"}, {CourseRole,"course"},
        {DepartmentRole,"department"}, {YearLevelRole,"yearLevel"},
        {StatusRole,"status"}, {VisitsRole,"visits"}, {SelectedRole,"selected"}
    };
}

void StudentsTableModel::setRecords(const QList<StudentRecord> &records)
{
    beginResetModel();
    m_records = records;
    // Prune selection to ids still present (selection survives by schoolId).
    QSet<QString> present;
    present.reserve(records.size());
    for (const StudentRecord &r : records) present.insert(r.schoolId);
    m_selected.intersect(present);
    endResetModel();
    emit countChanged();
    emit selectionChanged();
}

QStringList StudentsTableModel::selectedIds() const
{
    return QStringList(m_selected.begin(), m_selected.end());
}

void StudentsTableModel::emitRowSelectionChanged(int row)
{
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {SelectedRole});
}

void StudentsTableModel::toggle(const QString &schoolId)
{
    if (m_selected.contains(schoolId)) m_selected.remove(schoolId);
    else m_selected.insert(schoolId);
    for (int i = 0; i < m_records.size(); ++i)
        if (m_records.at(i).schoolId == schoolId) { emitRowSelectionChanged(i); break; }
    emit selectionChanged();
}

void StudentsTableModel::setAllSelected(bool on)
{
    m_selected.clear();
    if (on) for (const StudentRecord &r : m_records) m_selected.insert(r.schoolId);
    if (!m_records.isEmpty())
        emit dataChanged(index(0,0), index(m_records.size()-1,0), {SelectedRole});
    emit selectionChanged();
}

void StudentsTableModel::clearSelection() { setAllSelected(false); }
