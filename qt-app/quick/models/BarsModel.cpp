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

bool BarsModel::sameLabels(const QList<Bar> &bars) const
{
    if (bars.size() != m_bars.size())
        return false;
    for (int i = 0; i < bars.size(); ++i)
        if (bars.at(i).label != m_bars.at(i).label)
            return false;
    return true;
}

void BarsModel::setBars(const QList<Bar> &bars)
{
    // Fast path: same labels, same order, same count — only values moved.
    // Update in place via dataChanged so the QML delegates (and their
    // Behaviors on width / Layout.preferredHeight) stay alive across the
    // refresh instead of being destroyed and recreated by a model reset,
    // which would silently defeat those Behaviors every time.
    if (sameLabels(bars)) {
        m_bars = bars;
        double newMax = 0.0;
        for (const Bar &b : m_bars)
            if (b.value > newMax)
                newMax = b.value;
        m_maxValue = newMax;

        if (!m_bars.isEmpty())
            emit dataChanged(index(0, 0), index(m_bars.size() - 1, 0), { ValueRole });
        emit maxValueChanged();
        return;
    }

    // Slow path: the label set genuinely changed (different count, a
    // different label at some index, or empty<->non-empty) — rows must
    // appear/disappear, so a full reset is required.
    beginResetModel();
    m_bars = bars;
    m_maxValue = 0.0;
    for (const Bar &b : m_bars)
        if (b.value > m_maxValue)
            m_maxValue = b.value;
    endResetModel();
    emit maxValueChanged();
}
