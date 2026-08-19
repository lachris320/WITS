#include "RankingModel.h"

RankingModel::RankingModel(QObject *parent) : QAbstractListModel(parent) {}

int RankingModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant RankingModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const RankingEntry &e = m_entries.at(index.row());
    switch (role) {
    case RankRole:     return e.rank;
    case LabelRole:    return e.label;
    case SublabelRole: return e.sublabel;
    case VisitsRole:   return e.visits;
    case PercentRole:  return e.percentOfTotal;
    default:           return {};
    }
}

QHash<int, QByteArray> RankingModel::roleNames() const {
    return { {RankRole, "rank"}, {LabelRole, "label"}, {SublabelRole, "sublabel"},
             {VisitsRole, "visits"}, {PercentRole, "percent"} };
}

void RankingModel::setEntries(const QList<RankingEntry> &entries) {
    beginResetModel();
    m_entries = entries;
    endResetModel();
    emit countChanged();
}
