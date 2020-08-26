#include <ticket.h>
#include <validation.h>
#include <core_io.h>
#include <chainparams.h>
#include <script/standard.h>
#include <util/system.h>
#include <primitives/transaction.h>
#include <key.h>
#include <logging.h>

#include <vector>

using namespace std;

CScript GenerateTicketScript(const CKeyID keyid, const int lockHeight)
{
    auto script = CScript() << CScriptNum(lockHeight) << OP_CHECKLOCKTIMEVERIFY << OP_DROP << OP_DUP << OP_HASH160 << ToByteVector(keyid) << OP_EQUALVERIFY << OP_CHECKSIG;
    return std::move(script);
}

bool DecodeTicketScript(const CScript redeemScript, CKeyID& keyID, int &lockHeight)
{
    CScriptBase::const_iterator pc = redeemScript.begin();
    opcodetype opcodeRet;
    vector<unsigned char> vchRet;
    if (redeemScript.GetOp(pc, opcodeRet, vchRet)) {
        lockHeight = CScriptNum(vchRet, true).getint();
        if (redeemScript.GetOp(pc, opcodeRet, vchRet) 
            && redeemScript.GetOp(pc, opcodeRet, vchRet) 
            && redeemScript.GetOp(pc, opcodeRet, vchRet) 
            && redeemScript.GetOp(pc, opcodeRet, vchRet)
            && redeemScript.GetOp(pc, opcodeRet, vchRet)) {
            if (vchRet.size() == 20) {
                keyID = CKeyID(uint160(vchRet));
                return true;
            }
        }
    }
    return false;
}

bool GetPublicKeyFromScript(const CScript script, CPubKey &pubkey)
{
    CScriptBase::const_iterator pc = script.begin();
    opcodetype opcodeRet;
    vector<unsigned char> vchRet;
    if (script.GetOp(pc, opcodeRet, vchRet) && CScriptNum(vchRet,true)> 0) {
        vchRet.clear();
        if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_CHECKLOCKTIMEVERIFY) {
            vchRet.clear();
            if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_DROP) {
                vchRet.clear();
                if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_DUP) {
					vchRet.clear();
					if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_HASH160) {
						vchRet.clear();
						if (script.GetOp(pc, opcodeRet, vchRet) && vchRet.size() == 20) {
							if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_EQUALVERIFY) {
								vchRet.clear();
								if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_CHECKSIG) {
									vchRet.clear();
									return true;
								}
							}
						}
					}
                }
            }
        }
    }
    return false;
}

bool GetRedeemFromScript(const CScript script, CScript& redeemscript)
{
	CScriptBase::const_iterator pc = script.begin();
	opcodetype opcodeRet;
	vector<unsigned char> vchRet;
	if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_RETURN) {
		vchRet.clear();
		if (script.GetOp(pc, opcodeRet, vchRet)) {
			vchRet.clear();
			if (script.GetOp(pc, opcodeRet, vchRet)) {
				redeemscript = CScript(vchRet.begin(),vchRet.end());
				return true;
			}
		}
	}
	return false;
}

CTicket::CTicket(const COutPoint& out, const CAmount nValue, const CScript& redeemScript, const CScript &scriptPubkey)
    :out(new COutPoint(out)), redeemScript(redeemScript), scriptPubkey(scriptPubkey), nValue(nValue)
{
	CScriptBase::const_iterator pc = scriptPubkey.begin();
	opcodetype opcodeRet;
	vector<unsigned char> vchRet;
	CScriptID scriptID;
	if (scriptPubkey.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_HASH160) {
		vchRet.clear();
		if (scriptPubkey.GetOp(pc, opcodeRet, vchRet)) {
			scriptID = CScriptID(uint160(vchRet));
		}
	}
	// check the redeemScript and scriptPubkey, if unmatch throw
	if (scriptID!=CScriptID(redeemScript))
		throw error("error: unmatched redeemScript and scriptPubkey!");
}

CTicket::CTicket(const CTicket& other) : out(new COutPoint(*(other.out)))
{
    nValue = other.nValue;
    redeemScript = other.redeemScript;
    scriptPubkey = other.scriptPubkey;
}

CTicket::CTicket():out(new COutPoint) {

}

CTicket::~CTicket()
{
    if (out) delete out;
}

template <typename Stream>
void CTicket::Serialize(Stream& s) const
{
    s << out->hash;
    s << out->n;
    s << nValue;
    s << redeemScript;
    s << scriptPubkey;
}

template <typename Stream>
void CTicket::Unserialize(Stream& s) 
{
    s >> out->hash;
    s >> out->n;
    s >> nValue;
    s >> redeemScript;
    s >> scriptPubkey;
}

CTicket::CTicketState CTicket::State(int activeHeight) const
{
	int height = LockTime();
	if (height!=0){
		if (height > activeHeight){
			return CTicketState::IMMATURATE;
		}else if(height<=(activeHeight) && (activeHeight)<(height + Params().SlotLength())) {
			return CTicketState::USEABLE;
		}else{
			return CTicketState::OVERDUE;
		}
	}
	return CTicketState::UNKNOW;
}

int CTicket::LockTime() const
{
	CScriptBase::const_iterator pc = redeemScript.begin();
	opcodetype opcodeRet;
	vector<unsigned char> vchRet;
	if (redeemScript.GetOp(pc, opcodeRet, vchRet) && CScriptNum(vchRet,true)> 0) {
		auto height = CScriptNum(vchRet, false).getint();
		return height;
	}
	return 0;
}

CKeyID CTicket::KeyID() const
{
    CKeyID keyID;
    int lockHeight = 0;
    DecodeTicketScript(redeemScript, keyID, lockHeight);
    return keyID;
}

bool CTicket::Invalid() const 
{
	CScriptBase::const_iterator pc = redeemScript.begin();
	opcodetype opcodeRet;
	vector<unsigned char> vchRet;
	if (redeemScript.GetOp(pc, opcodeRet, vchRet) && CScriptNum(vchRet,true)> 0) {
		vchRet.clear();
		if (redeemScript.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_CHECKLOCKTIMEVERIFY) {
			vchRet.clear();
			if (redeemScript.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_DROP) {
				vchRet.clear();
				if (redeemScript.GetOp(pc, opcodeRet, vchRet) && vchRet.size() == 33) {
					vchRet.clear();
					if (redeemScript.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_CHECKSIG) {
						return false;
					}   
				}
			}
		}
	}
	return true;
}

CAmount CTicketView::BaseTicketPrice = 14300 * COIN;
CAmount nSlotLowerBoundTickerPrice = 1430 * COIN;
static const char DB_TICKET_SYNCED_KEY = 'S';
static const char DB_TICKET_SLOT_KEY = 'L';
static const char DB_TICKET_ADDR_KEY = 'A';
static const char DB_TICKET_HEIGHT_KEY = 'H';

void CTicketView::ConnectBlock(const int height, const CBlock &blk, CheckTicketFunc checkTicket)
{
    LogPrint(BCLog::FIRESTONE, "%s: height:%d\n", __func__, height);
    updateTicketPrice(height);
    std::vector<CTicket> tickets;
    for (auto tx : blk.vtx) {        
        if (!tx->IsTicketTx())
            continue;
        auto ticket = tx->Ticket();
        if( !checkTicket(height, ticket)) {
            LogPrint(BCLog::FIRESTONE, "%s: CheckTicket failure, hash:%s:%d\n", __func__, ticket->out->hash.ToString(), ticket->out->n);
            continue;
        }
        tickets.emplace_back(*ticket);
        ticketsInSlot[slotIndex].emplace_back(ticket);
        ticketsInAddr[ticket->KeyID()].emplace_back(ticket);
        LogPrint(BCLog::FIRESTONE, "%s: detected a new firestone, height:%d, hash:%s:%d\n", __func__, height, ticket->out->hash.ToString(), ticket->out->n);
    } 
    if (tickets.size() > 0) {
        if (!WriteTicketsToDisk(height, tickets)) {
            LogPrint(BCLog::FIRESTONE, "%s: WriteTicketsToDisk retrun false, height:%d\n", __func__, height);
        }
    }
}

void CTicketView::DisconnectBlock(const int height, const CBlock &blk)
{
    LogPrint(BCLog::FIRESTONE, "%s: height:%d, block:%s\n", __func__, height, blk.GetHash().ToString());
    auto key = std::make_pair(DB_TICKET_HEIGHT_KEY, height);
    Erase(key, true);
    ticketsInSlot.clear();
    ticketsInAddr.clear();
    slotIndex = 0;
    ticketPrice = BaseTicketPrice;
    for (auto i = 0; i < height; i++) {
        LoadTicketFromDisk(i);
    }
}

CAmount CTicketView::CurrentTicketPrice() const
{
    return ticketPrice;
}

std::vector<CTicketRef> CTicketView::CurrentSlotTicket()
{
    return ticketsInSlot[slotIndex];
}

std::vector<CTicketRef> CTicketView::GetTicketsBySlotIndex(const int slotIndex) 
{
    return ticketsInSlot[slotIndex];
}

std::vector<CTicketRef> CTicketView::FindeTickets(const CKeyID key)
{
    return ticketsInAddr[key];
}

const int CTicketView::SlotLength()
{
    static int slotLength= Params().SlotLength();
    return slotLength;
}

const int CTicketView::LockTime()
{
    return (slotIndex + 1) * SlotLength() - 1;
}

const int CTicketView::LockTime(const int index)
{
    return std::max((index + 1) * SlotLength() - 1, SlotLength() -1);
}

CTicketView::CTicketView(size_t nCacheSize, bool fMemory, bool fWipe) 
    :CDBWrapper(GetDataDir() / "ticket", nCacheSize, fMemory, fWipe),
    ticketPrice(BaseTicketPrice),
    slotIndex(0) 
{
}

bool CTicketView::WriteTicketsToDisk(const int height, const std::vector<CTicket> &tickets)
{
    return Write(std::make_pair(DB_TICKET_HEIGHT_KEY, height), tickets);
}

bool CTicketView::LoadTicketFromDisk(const int height)
{
    updateTicketPrice(height);
    auto key = std::make_pair(DB_TICKET_HEIGHT_KEY, height);
    if (Exists(key)) {
        std::vector<CTicket> tickets;
        if (!Read(key, tickets)) {
            LogPrint(BCLog::FIRESTONE, "%s: Read retrun false, height:%d\n", __func__, height);
            return false;
        }
        for (auto ticket : tickets) {
            CTicketRef t;
            t.reset(new CTicket(ticket));
            ticketsInSlot[slotIndex].emplace_back(t);
            ticketsInAddr[ticket.KeyID()].emplace_back(t);
        }
    }
    return true;
}

CAmount CTicketView::TicketPriceInSlot(const int index)
{
    CAmount price = BaseTicketPrice;
    for (auto i = 0; i < index; i++) {
        if (ticketsInSlot[i].size() > SlotLength()) {
            price *= 1.05;
        }
        else if (ticketsInSlot[i].size() < SlotLength()) {
            price = std::max(CAmount(price * 0.95), i * nSlotLowerBoundTickerPrice);
        }
        price = (i + 1) > 5 ? std::max(BaseTicketPrice, price) : BaseTicketPrice;
    }
    return price;
}

void CTicketView::updateTicketPrice(const int height)
{
    const auto len = Params().SlotLength();
    if (height % len == 0 && height != 0) { //update ticket price
        auto prevSlotTicketSize = ticketsInSlot[slotIndex].size();
        if (prevSlotTicketSize > len) {
            ticketPrice *= 1.05;
        }
        else if (prevSlotTicketSize < len) {
            ticketPrice = std::max(CAmount(ticketPrice * 0.95), slotIndex * nSlotLowerBoundTickerPrice);
        }
        slotIndex = int(height / len);
        ticketPrice = slotIndex > 5 ? std::max(BaseTicketPrice, ticketPrice) : BaseTicketPrice;
        LogPrint(BCLog::FIRESTONE, "%s: updata ticket slot, index:%d, price:%d, prevSlotTicketCount:%d\n", __func__, slotIndex, ticketPrice, prevSlotTicketSize);
    }
}
