#include "SearchResultsModel.h"

#include "Initials.h"
#include "apiconfig.h"

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
    // Computed, not stored: search_students.php returns no photo data (see
    // Phase 3 owner decision — 171 students, 0 usable photos on disk), so
    // the avatar chip is initials-only for now. Deriving here means a future
    // photo role can slot in without touching StudentRecord/the parser.
    case InitialsRole:   return Initials::of(r.name);
    case PhotoRole:
        // Approach A: the ONLY place a search photo becomes absolute. Never join
        // an empty path (ApiConfig::endpoint("") yields the bare base URL, which
        // would force a needless load-failure) — return empty so LAvatar shows
        // initials.
        return r.photo.isEmpty() ? QString()
                                 : ApiConfig::endpoint(r.photo).toString();
    default:             return {};
    }
}

QHash<int, QByteArray> SearchResultsModel::roleNames() const
{
    return {
        { NameRole, "name" }, { SchoolIdRole, "schoolId" }, { CourseRole, "course" },
        { DepartmentRole, "department" }, { YearLevelRole, "yearLevel" },
        { StatusRole, "status" }, { VisitsRole, "visits" },
        { InitialsRole, "initials" }, { PhotoRole, "photo" },
    };
}

void SearchResultsModel::setRecords(const QList<StudentRecord> &records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
    emit countChanged();
}
