#ifndef TICKETPAGE_H
#define TICKETPAGE_H

#include <QWidget>

namespace Ui {
class TicketPage;
}


class PlatformStyle;
class WalletModel;
class CKeyID;

class TicketPage : public QWidget
{
    Q_OBJECT

public:
    explicit TicketPage(const PlatformStyle* style, QWidget *parent = nullptr);
    ~TicketPage();

    void setWalletModel(WalletModel *walletModel);

    struct ticketInfo {
        size_t totalCount;
        size_t curSlotCount;
        size_t nextSlotCount;
        size_t overdueCount;
    };

    void updateData();

private Q_SLOTS:
    void on_btnQuery_clicked();

    ticketInfo getticketInfo(const CKeyID& key);

    void on_btnBuy_clicked();

    void on_btnRelease_clicked();

private:
    Ui::TicketPage *ui;
    WalletModel *_walletModel;
};

#endif // TICKETPAGE_H
