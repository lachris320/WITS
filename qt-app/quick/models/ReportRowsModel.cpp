#include "ReportRowsModel.h"

int reportVisits(const QJsonObject &row)
{
    const QJsonValue v = row.value(QStringLiteral("visits"));
    if (v.isDouble())
        return v.toInt();
    return v.toString().toInt();   // "5" -> 5; missing/"" -> 0
}

ReportRowsModel::ReportRowsModel(QObject *parent) : QAbstractListModel(parent) {}

int ReportRowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant ReportRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case NameRole:      return r.name;
    case CourseRole:    return r.course;
    case YearLevelRole: return r.year;
    case VisitsRole:    return r.visits;
    default:            return {};
    }
}

QHash<int, QByteArray> ReportRowsModel::roleNames() const
{
    return { { NameRole, "name" }, { CourseRole, "course" },
             { YearLevelRole, "year" }, { VisitsRole, "visits" } };
}

void ReportRowsModel::setRows(const QJsonArray &data)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(data.size());
    for (const QJsonValue &v : data) {
        const QJsonObject o = v.toObject();
        m_rows.append({ o.value("name").toString(),
                        o.value("course").toString(),
                        o.value("year_level").toString(),
                        reportVisits(o) });
    }
    endResetModel();
    emit countChanged();
}
