#include <qt/ticketpage.h>
#include <qt/forms/ui_ticketpage.h>

#include <QList>
#include <QDebug>
#include <qt/walletmodel.h>
#include <qt/guiutil.h>
#include <key_io.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <univalue.h>
#include <node/transaction.h>
#include <txmempool.h>
#include <amount.h>

struct SlotInfo
{
    int index;
    int lockTime;
    CAmount price;
    uint64_t count;

    static SlotInfo currentSlotInfo();
};

SlotInfo SlotInfo::currentSlotInfo()
{
    LOCK(cs_main);

    int index = pticketview->SlotIndex();
    auto price = pticketview->TicketPriceInSlot(index);
    auto count = (uint64_t)pticketview->GetTicketsBySlotIndex(index).size();
    auto lockTime = pticketview->LockTime(index);

    return SlotInfo {
        index, lockTime, price, count
    };
}

TicketPage::TicketPage(const PlatformStyle* style, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TicketPage)
{
    ui->setupUi(this);
}

TicketPage::~TicketPage()
{
    delete ui;
}

void TicketPage::setWalletModel(WalletModel *walletModel)
{
    _walletModel = walletModel;
    updateData();
}

void TicketPage::updateData()
{
    if (!_walletModel || _walletModel->wallet().isLocked()) {
        return;
    }

    auto slotInfo = SlotInfo::currentSlotInfo();
    ui->ebSlotIdx->setText(QString("%1").arg(slotInfo.index));
    ui->ebticketCount->setText(QString("%1").arg(slotInfo.count));
    ui->ebLockTime->setText(QString("%1").arg(slotInfo.lockTime));
    ui->ebTicketPrice->setText(GUIUtil::formatPrice(*_walletModel, slotInfo.price));
}

void TicketPage::on_btnQuery_clicked()
{
    auto fromAddr = ui->ebQueryAddress->text().trimmed();
    CTxDestination fromDest = DecodeDestination(fromAddr.toStdString());
    if (!IsValidDestination(fromDest) || fromDest.type() != typeid(PKHash)) {
      QMessageBox::warning(this, windowTitle(), tr("Invalid address"), QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    qInfo() << "here1" << endl;
    auto ticketInfo = getticketInfo(CKeyID(boost::get<PKHash>(fromDest)));

    auto strTotal = tr("Address \"%1\" has %2 tickets").arg(fromAddr).arg(ticketInfo.totalCount);
    auto strCurSlot = tr("%1 ticket(s) in current slot").arg(ticketInfo.curSlotCount);
    auto strNextSlot = tr("%1 immature ticket(s)").arg(ticketInfo.nextSlotCount);
    auto strOverdue = tr("%1 overdue ticket(s)").arg(ticketInfo.overdueCount);

    auto parts = QList<QString>() << strTotal << strCurSlot << strNextSlot << strOverdue;
    auto message = parts.join("\n");

    QMessageBox::information(this, tr("ticket Information"), message, QMessageBox::Ok, QMessageBox::Ok);
}

TicketPage::ticketInfo TicketPage::getticketInfo(const CKeyID &key)
{
    std::vector<CTicketRef> alltickets;
    {
      //
      // scope to limit the lock scope...
      LOCK(cs_main);
      alltickets = pticketview->FindTickets(key);
      auto end = std::remove_if(alltickets.begin(), alltickets.end(), [](const CTicketRef& ticket) {
        return ::ChainstateActive().CoinsTip().AccessCoin(COutPoint(ticket->out->hash, ticket->out->n)).IsSpent();
      });
      alltickets.erase(end, alltickets.end());
    }

    size_t useableCount = 0;
    size_t overdueCount = 0;
    size_t immatureCount = 0;

    std::for_each(alltickets.begin(), alltickets.end(), [&useableCount, &overdueCount, &immatureCount](const CTicketRef& ticket) {
        int height = ticket->LockTime();
        auto keyid = ticket->KeyID();
        if (keyid.size() == 0 || height == 0) {
            return;
        }
        switch (ticket->State(ChainActive().Tip()->nHeight)){
        case CTicket::CTicketState::IMMATURATE:
            immatureCount++;
          break;
        case CTicket::CTicketState::USEABLE:
            useableCount++;
          break;
        case CTicket::CTicketState::OVERDUE:
            overdueCount++;
          break;
        case CTicket::CTicketState::UNKNOW:
          break;
        }
    });
    ticketInfo info = {
      alltickets.size(), useableCount, immatureCount, overdueCount
    };
    return info;
}

void TicketPage::on_btnBuy_clicked()
{
  if(!_walletModel || _walletModel->wallet().isLocked()) {
    QMessageBox::warning(this, windowTitle(), tr("Please unlock wallet to continue"), QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  auto buyAddr = ui->ebBuyAddress->text().trimmed();
  CTxDestination buyDest = DecodeDestination(buyAddr.toStdString());
  if (!IsValidDestination(buyDest) || buyDest.type() != typeid(PKHash)) {
    QMessageBox::warning(this, windowTitle(), tr("Invalid target address"), QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  auto changeAddr = ui->ebChangeAddress->text().trimmed();
  CTxDestination changeDest = DecodeDestination(changeAddr.toStdString());
  if (!IsValidDestination(changeDest) || changeDest.type() != typeid(PKHash)) {
    QMessageBox::warning(this, windowTitle(), tr("Invalid change address"), QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  auto buyID = CKeyID(boost::get<PKHash>(buyDest));
  auto changeID = PKHash(boost::get<PKHash>(changeDest));

  if (!_walletModel->wallet().isSpendable(buyDest)) {
    auto ret = QMessageBox::warning(this, windowTitle(), tr("Target address isn't an address in your wallet, are you sure?"), QMessageBox::Yes, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return;
    }
  }

  if (!_walletModel->wallet().isSpendable(changeDest)) {
    auto ret = QMessageBox::warning(this, windowTitle(), tr("Change address isn't an address in your wallet, are you sure?"), QMessageBox::Yes, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return;
    }
  }

  try {
      _walletModel->wallet().doWithChainAndWalletLock([&](std::unique_ptr<interfaces::Chain::Lock>& lockedChain, interfaces::Wallet& iWallet) {
        LOCK(cs_main);

        auto locktime = pticketview->LockTime();
        if (locktime == ChainActive().Height()) {
          QMessageBox::warning(this, windowTitle(), tr("Can't buy ticket on slot's last block"), QMessageBox::Ok, QMessageBox::Ok);
          return;
        }

        if (pticketview->SlotIndex() < 1) {
          QMessageBox::warning(this, windowTitle(), tr("Can't buy ticket on 0 ~ 4 slots."), QMessageBox::Ok, QMessageBox::Ok);
          return;
        }
        auto nAmount = pticketview->CurrentTicketPrice();
        auto redeemScript = GenerateTicketScript(buyID, locktime);
        buyDest = CTxDestination(ScriptHash(redeemScript));
        auto scriptPubkey = GetScriptForDestination(buyDest);
        auto opRetScript = CScript() << OP_RETURN << CTicket::VERSION << ToByteVector(redeemScript);

        //set change dest
        CCoinControl coin_control;
        coin_control.destChange = CTxDestination(changeID);

        iWallet.sendMoneyWithOpRet(*lockedChain, buyDest, nAmount, false, opRetScript, coin_control);
      });

    QMessageBox::information(this, windowTitle(), tr("Buy ticket success!"), QMessageBox::Ok, QMessageBox::Ok);
  } catch(...) {
    QMessageBox::critical(this, windowTitle(), tr("Failed to buy ticket, please make sure there is enough balance in your wallet"), QMessageBox::Ok, QMessageBox::Ok);
  }
}

void TicketPage::on_btnRelease_clicked()
{
    if(!_walletModel || _walletModel->wallet().isLocked()) {
      QMessageBox::warning(this, windowTitle(), tr("Please unlock wallet to continue"), QMessageBox::Ok, QMessageBox::Ok);
      return;
    }
    auto relAddr = ui->ebReleaseAddress->text().trimmed();
    CTxDestination relDest = DecodeDestination(relAddr.toStdString());
    if (!IsValidDestination(relDest) || relDest.type() != typeid(PKHash)) {
      QMessageBox::warning(this, windowTitle(), tr("Invalid address"), QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    auto refundAddr = ui->ebRefundAddress->text().trimmed();
    CTxDestination refundDest = DecodeDestination(refundAddr.toStdString());
    if (!IsValidDestination(refundDest) || refundDest.type() != typeid(PKHash)) {
      QMessageBox::warning(this, windowTitle(), tr("Invalid refund address"), QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    auto relID = CKeyID(boost::get<PKHash>(relDest));
    if (!_walletModel->wallet().isSpendable(relDest)) {
      QMessageBox::warning(this, windowTitle(), tr("This address isn't your address, you can't release ticket for it"), QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    std::vector<CTicketRef> alltickets;
    {
      LOCK(cs_main);
      alltickets = pticketview->FindTickets(CKeyID(boost::get<PKHash>(relDest)));
    }

    std::vector<CTicketRef> tickets;
    for(size_t i=0;i < alltickets.size(); i++){
        auto ticket = alltickets[i];
        auto out = *(ticket->out);
        if (!::ChainstateActive().CoinsTip().AccessCoin(out).IsSpent() && !mempool.isSpent(out)) {
            tickets.push_back(ticket);
            if (tickets.size() > 4)
                break;
        }
    }
    if(tickets.empty()) {
      QMessageBox::information(this, windowTitle(), tr("No tickets in this address"), QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    std::map<uint256,std::pair<int,CScript>> txScriptInputs;
    std::vector<CTxOut> outs;
    UniValue ticketids(UniValue::VARR);
    for(auto iter = tickets.begin(); iter!=tickets.end(); iter++){
      auto state = (*iter)->State(::ChainActive().Height());
      if (state == CTicket::CTicketState::OVERDUE){
        auto ticket = (*iter);
        uint256 txid = ticket->out->hash;
        uint32_t n = ticket->out->n;
        CScript redeemScript = ticket->redeemScript;
        ticketids.push_back(txid.ToString() + ":" + std::to_string(n));

        // construct the freeticket tx inputs.
        auto prevTx = MakeTransactionRef();
        uint256 hashBlock;
        if (!GetTransaction(txid, prevTx, Params().GetConsensus(), hashBlock)) {
            QMessageBox::critical(this, windowTitle(), tr("Failed to free ticket, something unexpected happened..."), QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
        txScriptInputs.insert(std::make_pair(txid,std::make_pair(n,redeemScript)));
        outs.push_back(prevTx->vout[n]);
      }
    }

    CKey vchSecret;
    if (!_walletModel->wallet().getPrivKey(relID, vchSecret)) {
        QMessageBox::critical(this, windowTitle(), tr("Failed to free ticket, something unexpected happened... %1").arg("Can't get Private Key"), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    if (txScriptInputs.empty()) {
        QMessageBox::critical(this, windowTitle(), tr("No expired ticket found."), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    auto tx = _walletModel->wallet().createTicketAllSpendTx(txScriptInputs, outs, refundDest, vchSecret);

    std::string errStr;
    uint256 spendTxID;
    const CAmount max_fee{ _walletModel->wallet().getDefaultMaxTxFee() };
    if (!_walletModel->wallet().broadcastTransaction(tx, max_fee, true, errStr)) {
      QMessageBox::warning(this, windowTitle(), tr("Failed to broadcast transation! %1").arg(errStr.c_str()), QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    QMessageBox::information(this, windowTitle(), tr("Released %1 ticket(s)").arg(tickets.size()),
                             QMessageBox::Ok, QMessageBox::Ok);
}
