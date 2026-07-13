#ifndef VISITLOGROWSMODEL_H
#define VISITLOGROWSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

// Visit Logs row model (spec §6.3/§7.3). Roles are the UNION of the student
// and guest column sets; unused roles are empty per mode. The screen selects
// which columns to show from VisitLogsViewModel.mode.
class VisitLogRowsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        DateRole = Qt::UserRole + 1,
        NameRole,
        CourseRole,
        DepartmentRole,
        TimeInRole,
        TimeOutRole,
        CompanyRole,
        PurposeRole,
    };
    struct Row {
        QString date, name, course, department, timeIn, timeOut, company, purpose;
    };

    explicit VisitLogRowsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(const QList<Row> &rows);

private:
    QList<Row> m_rows;
};

#endif // VISITLOGROWSMODEL_H
