#ifndef GUESTWINDOW_H
#define GUESTWINDOW_H

#include <QDialog>
#include <QNetworkAccessManager>

namespace Ui {
class guestwindow; // matches <class>guestwindow</class> in your .ui
}

class GuestWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GuestWindow(QWidget *parent = nullptr);
    ~GuestWindow();

private slots:
    void onSubmitBtnClicked();
    void onCancelBtnClicked();

private:
    Ui::guestwindow *ui;              // note lowercase 'guestwindow'
    QNetworkAccessManager *networkManager;
};

#endif // GUESTWINDOW_H
