#ifndef STUDENTSTABLEMODEL_H
#define STUDENTSTABLEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QStringList>
#include "studentdata.h"

// Multi-select student table model (spec §4.2). Selection is tracked by
// schoolId in a QSet so it SURVIVES a data refresh (setRecords) — a re-filter
// or reload must not silently drop the operator's selection.
class StudentsTableModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool allSelected READ allSelected NOTIFY selectionChanged)
    Q_PROPERTY(bool anySelected READ anySelected NOTIFY selectionChanged)
public:
    enum Roles {
        NameRole = Qt::UserRole + 1, SchoolIdRole, CourseRole, DepartmentRole,
        YearLevelRole, StatusRole, VisitsRole, SelectedRole
    };
    explicit StudentsTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_records.size(); }
    int selectedCount() const { return m_selected.size(); }
    bool anySelected() const { return !m_selected.isEmpty(); }
    bool allSelected() const { return !m_records.isEmpty() && m_selected.size() == m_records.size(); }

    void setRecords(const QList<StudentRecord> &records);
    QStringList selectedIds() const;

    Q_INVOKABLE void toggle(const QString &schoolId);
    Q_INVOKABLE void setAllSelected(bool on);
    Q_INVOKABLE void clearSelection();

signals:
    void countChanged();
    void selectionChanged();

private:
    void emitRowSelectionChanged(int row);
    QList<StudentRecord> m_records;
    QSet<QString> m_selected;   // selected schoolIds
};

#endif // STUDENTSTABLEMODEL_H
