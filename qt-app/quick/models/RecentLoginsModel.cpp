#include "RecentLoginsModel.h"

RecentLoginsModel::RecentLoginsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RecentLoginsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant RecentLoginsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case NameRole:      return r.name;
    case CourseRole:    return r.course;
    case YearShortRole: return r.yearShort;
    case DeptRole:      return r.dept;
    case TimeRole:      return r.time;
    case InitialsRole:  return r.initials;
    case FreshRole:     return r.fresh;
    default:            return {};
    }
}

QHash<int, QByteArray> RecentLoginsModel::roleNames() const
{
    return {
        { NameRole,      "name" },
        { CourseRole,    "course" },
        { YearShortRole, "yearShort" },
        { DeptRole,      "dept" },
        { TimeRole,      "time" },
        { InitialsRole,  "initials" },
        { FreshRole,     "fresh" },
    };
}

QString RecentLoginsModel::shortenYear(const QString &year)
{
    QString y = year;
    return y.replace(QLatin1String(" Year"), QLatin1String(" Yr"));
}

QString RecentLoginsModel::initialsOf(const QString &name)
{
    QString out;
    const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        out.append(p.at(0).toUpper());
        if (out.size() == 2)
            break;
    }
    return out;
}

void RecentLoginsModel::prepend(const QString &name, const QString &course,
                                const QString &year, const QString &dept,
                                const QString &time)
{
    // Clear the previous fresh flag(s) so only the incoming row highlights.
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].fresh) {
            m_rows[i].fresh = false;
            emit dataChanged(index(i), index(i), { FreshRole });
        }
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_rows.prepend(Row{ name, course, shortenYear(year), dept, time,
                        initialsOf(name), true });
    endInsertRows();

    if (m_rows.size() > kMaxRows) {
        beginRemoveRows(QModelIndex(), kMaxRows, m_rows.size() - 1);
        m_rows.erase(m_rows.begin() + kMaxRows, m_rows.end());
        endRemoveRows();
    }
}
