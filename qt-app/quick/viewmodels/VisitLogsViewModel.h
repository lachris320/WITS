#ifndef VISITLOGSVIEWMODEL_H
#define VISITLOGSVIEWMODEL_H

#include <QByteArray>
#include <QDate>
#include <QObject>
#include <QString>
#include <qqml.h>
#include "VisitLogRowsModel.h"

class QNetworkAccessManager;

// Visit Logs screen VM (spec §6.3/§7.3). Student attendance is the primary
// view (get_library_visits.php); Guest is a secondary toggle (get_visitors.php
// reused, driven only with app-generated ISO dates, §5.4).
class VisitLogsViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(Range range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(int count READ count NOTIFY dataChanged)
    Q_PROPERTY(QString rangeLabel READ rangeLabel NOTIFY dataChanged)
    Q_PROPERTY(VisitLogRowsModel *rows READ rows CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    enum Mode { Student, Guest };
    Q_ENUM(Mode)
    enum Range { Today, Week };
    Q_ENUM(Range)

    explicit VisitLogsViewModel(QObject *parent = nullptr);

    Mode mode() const { return m_mode; }
    Range range() const { return m_range; }
    int count() const { return m_count; }
    QString rangeLabel() const { return m_rangeLabel; }
    VisitLogRowsModel *rows() { return &m_rows; }
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }

    void setMode(Mode m);
    void setRange(Range r);

    Q_INVOKABLE void refresh();

    // Network-free seams (tests + reply handlers).
    void applyStudentVisits(const QByteArray &raw);
    void applyGuestVisits(const QByteArray &raw);

    // In-flight request generation guard. refresh() calls nextRequestSeq()
    // when it issues a request and captures the returned value; the
    // reply-finished handler later calls isCurrentRequest(seq) before
    // touching m_rows/loading/errorText, so a reply superseded by a newer
    // refresh() (e.g. a mode/range switch clicked before the prior request
    // returned) is dropped instead of clobbering newer data. Exposed here
    // (not just private) so the counter's increment/compare arithmetic can
    // be pinned by a unit test without a real network round trip.
    quint64 nextRequestSeq();
    bool isCurrentRequest(quint64 seq) const;

    // Pure: Monday date -> "Jul 13 – Jul 19, 2026".
    static QString formatWeekLabel(const QDate &monday);

signals:
    void modeChanged();
    void rangeChanged();
    void dataChanged();
    void loadingChanged();
    void errorTextChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);
    void setCount(int c);
    QString computeRangeLabel() const;   // uses current date + m_range
    void resetDataOnError();   // clears rows/count, recomputes rangeLabel; shared by all error paths
    // Step back from `d` to this week's Monday (Qt: dayOfWeek() Monday==1).
    // Shared by computeRangeLabel() and refresh()'s guest/week branch so the
    // "Monday of this week" arithmetic can't drift between the two copies.
    static QDate weekMonday(const QDate &d);

    QNetworkAccessManager *m_nam = nullptr;
    VisitLogRowsModel m_rows;
    Mode m_mode = Student;
    Range m_range = Today;
    int m_count = 0;
    QString m_rangeLabel;
    bool m_loading = false;
    QString m_errorText;
    quint64 m_requestSeq = 0;
};

#endif // VISITLOGSVIEWMODEL_H
