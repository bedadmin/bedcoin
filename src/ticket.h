#ifndef BITCOIN_TICKET_H
#define BITCOIN_TICKET_H

#include <config/bitcoin-config.h>
#include <script/script.h>
#include <pubkey.h>
#include <amount.h>
#include <dbwrapper.h>

#include <functional>

CScript GenerateTicketScript(const CKeyID keyid, const int lockHeight);

bool DecodeTicketScript(const CScript redeemScript, CKeyID& keyID, int &lockHeight);

bool GetRedeemFromScript(const CScript script, CScript& redeemscript);

class COutPoint;
/**
 * A ticket entry.
 * One ticket is mapping into one transaction, which has redeemScript.
 * It means ticket should include:
 *   IN : this transaction's outpoint, which points to the ticket,
 *   OUT: nValue and scriptPubkey.
 */
class CTicket {
public:
    static const int32_t VERSION = 1;

    enum CTicketState {
        IMMATURATE = 0,
        USEABLE,
        OVERDUE,
        UNKNOW
    };

    COutPoint* out;
    CAmount nValue;
    CScript redeemScript;
    CScript scriptPubkey;

    CTicket(const COutPoint& out, const CAmount nValue, const CScript& redeemScript, const CScript &scriptPubkey);

    CTicket(const CTicket& other);

    CTicket();

    ~CTicket();

    CTicketState State(int activeHeight) const;

    int LockTime()const;

    CKeyID KeyID() const;

    bool Invalid() const;

    template <typename Stream>
    void Serialize(Stream& s) const;

    template <typename Stream>
    void Unserialize(Stream& s);
};

typedef std::shared_ptr<const CTicket> CTicketRef;
class CBlock;
typedef std::function<bool(const int, const CTicketRef&)> CheckTicketFunc;

/** 
 * Abstract view on the ticket dataset. 
 */
class CTicketView : public CDBWrapper {
public: 
    CTicketView(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    ~CTicketView() = default;
    
    /** 
     * ConnectBlock is an up-layer api, which is called by ConnectTip.
     * @param[in]    height       the block height, at which this ticket appears.
     * @param[out]   blk          the block, at which this ticket appears.
     * @param[in]    checkTicket  the function, which is used to check the ticket whether valid.
     */
    void ConnectBlock(const int height, const CBlock &blk, CheckTicketFunc checkTicket);

    void DisconnectBlock(const int height, const CBlock &blk);
    
    /** 
     * Show the current ticket price.
     * @return   the current ticket price.
     */
    CAmount CurrentTicketPrice() const;

    std::vector<CTicketRef>& CurrentSlotTicket();
    
    /** 
     * Find all ticket owned by the KeyID.
     */
    std::vector<CTicketRef>& FindTickets(const CKeyID& key);

    std::vector<CTicketRef>& GetTicketsBySlotIndex(const int slotindex);

    const int SlotIndex() const { return slotIndex; }
    
    /** 
     * Slotlenth is 2048 each slot.
     * @return   the current slotlength.
     */
    const int SlotLength();

    const int LockTime();
    
    /** 
     * 0 slot's locktime is 2047.
     * @param[in]  index, the slot index calculated by height/slotlength.
     * @return   the current lockheight, beyond which the ticket is USEABLE.
     */
    const int LockTime(const int index);
    
    /** 
     * Load the ticket set at height.
     * @param[in]   height, the block height, from which tickets are load.
     */
    bool LoadTicketFromDisk(const int height);

    CAmount TicketPriceInSlot(const int index);

private:
    bool WriteTicketsToDisk(const int height, const std::vector<CTicket> &tickets);
    
    /** 
     * Update the ticket price, by +5% or -5% one slot.
     * In default case, the slot length is 2048, so the input height is used to calculate
     * The slot:
     *             slot = height / slotlength.
     * 
     * When the last slot has more than slotlength tickets, the price is update:
     *                      Price *= 1.05;
     * On the other hand:
     *                      Price *= 0.95;
     */
    void updateTicketPrice(const int height);

private:
    /** This map records tickets in each slot, one slot is 2048 blocks.*/
    std::map<int, std::vector<CTicketRef>> ticketsInSlot;
    std::map<CKeyID, std::vector<CTicketRef>> ticketsInAddr;
    CAmount ticketPrice;
    int slotIndex;
    static CAmount BaseTicketPrice;
};

#endif
