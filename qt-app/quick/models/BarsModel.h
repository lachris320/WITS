#ifndef BARSMODEL_H
#define BARSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

// Generic bar-series model for LBarChart (hourly + department, spec §6.1).
// Carries only label + numeric value; the bar COLOR is chosen in QML from
// Theme tokens (Global Constraints: no color decisions in C++). Follows the
// RecentLoginsModel pattern (roles enum, roleNames(), a typed Row struct).
class BarsModel : public QAbstractListModel
{
    Q_OBJECT
    // maxValue MUST be a Q_PROPERTY with NOTIFY: LBarChart binds
    // `maxValue: vm.hourlyModel.maxValue` from QML, and the data arrives
    // asynchronously via setBars() long after the binding is created — a plain
    // C++ accessor is unreadable from QML and a non-notifying property would
    // latch the pre-data value (0) forever.
    Q_PROPERTY(double maxValue READ maxValue NOTIFY maxValueChanged)
public:
    struct Bar { QString label; double value = 0.0; };
    enum Roles { LabelRole = Qt::UserRole + 1, ValueRole };

    explicit BarsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setBars(const QList<Bar> &bars);
    double maxValue() const { return m_maxValue; }

signals:
    void maxValueChanged();

private:
    QList<Bar> m_bars;
    double m_maxValue = 0.0;
};

#endif // BARSMODEL_H
