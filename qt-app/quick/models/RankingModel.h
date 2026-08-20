#ifndef RANKINGMODEL_H
#define RANKINGMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "reportanalytics.h"   // RankingEntry

// One ranking table (Top 10 Students / Courses / Departments). Mirrors the
// BarsModel/ReportRowsModel pattern: roles enum, roleNames(), count property.
// Fed a QList<RankingEntry> computed by ReportAnalytics; no logic of its own.
class RankingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { RankRole = Qt::UserRole + 1, LabelRole, SublabelRole, VisitsRole, PercentRole };
    explicit RankingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_entries.size()); }
    void setEntries(const QList<RankingEntry> &entries);

signals:
    void countChanged();

private:
    QList<RankingEntry> m_entries;
};

#endif // RANKINGMODEL_H
