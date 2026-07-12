#ifndef RECENTLOGINSMODEL_H
#define RECENTLOGINSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

// The kiosk attendance feed. Newest first; the newest row is the only "fresh"
// one (drives the gold highlight). Capped at 40 rows to match the design feed.
class RecentLoginsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        CourseRole,
        YearShortRole,
        DeptRole,
        TimeRole,
        InitialsRole,
        FreshRole,
    };

    explicit RecentLoginsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void prepend(const QString &name, const QString &course,
                 const QString &year, const QString &dept, const QString &time);

private:
    struct Row {
        QString name, course, yearShort, dept, time, initials;
        bool fresh = false;
    };
    static QString shortenYear(const QString &year);   // "3rd Year" -> "3rd Yr"
    static QString initialsOf(const QString &name);    // "Maria Santos" -> "MS"

    QList<Row> m_rows;
    static constexpr int kMaxRows = 40;
};

#endif // RECENTLOGINSMODEL_H
