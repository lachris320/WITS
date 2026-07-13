#include "BarsModel.h"

BarsModel::BarsModel(QObject *parent) : QAbstractListModel(parent) {}

int BarsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_bars.size();
}

QVariant BarsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_bars.size())
        return {};
    const Bar &b = m_bars.at(index.row());
    switch (role) {
    case LabelRole: return b.label;
    case ValueRole: return b.value;
    default:        return {};
    }
}

QHash<int, QByteArray> BarsModel::roleNames() const
{
    return { { LabelRole, "label" }, { ValueRole, "value" } };
}

void BarsModel::setBars(const QList<Bar> &bars)
{
    beginResetModel();
    m_bars = bars;
    m_maxValue = 0.0;
    for (const Bar &b : m_bars)
        if (b.value > m_maxValue)
            m_maxValue = b.value;
    endResetModel();
    emit maxValueChanged();
}
