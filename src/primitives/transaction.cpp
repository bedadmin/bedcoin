// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>
#include <core_io.h>
#include <hash.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <script/standard.h>

#include <assert.h>

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != SEQUENCE_FINAL)
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0, 30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nLockTime(tx.nLockTime) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::ComputeHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::ComputeWitnessHash() const
{
    if (!HasWitness()) {
        return hash;
    }
    return SerializeHash(*this, SER_GETHASH, 0);
}

/* For backward compatibility, the hash is initialized to 0. TODO: remove the need for this default constructor entirely. */
CTransaction::CTransaction() : vin(), vout(), nVersion(CTransaction::CURRENT_VERSION), nLockTime(0), hash{}, m_witness_hash{} {}
CTransaction::CTransaction(const CMutableTransaction& tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nLockTime(tx.nLockTime), hash{ComputeHash()}, m_witness_hash{ComputeWitnessHash()} {}
CTransaction::CTransaction(CMutableTransaction&& tx) : vin(std::move(tx.vin)), vout(std::move(tx.vout)), nVersion(tx.nVersion), nLockTime(tx.nLockTime), hash{ComputeHash()}, m_witness_hash{ComputeWitnessHash()} {}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (const auto& tx_out : vout) {
        if (!MoneyRange(tx_out.nValue) || !MoneyRange(nValueOut + tx_out.nValue))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
        nValueOut += tx_out.nValue;
    }
    assert(MoneyRange(nValueOut));
    return nValueOut;
}

unsigned int CTransaction::GetTotalSize() const
{
    return ::GetSerializeSize(*this, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (const auto& tx_in : vin)
        str += "    " + tx_in.ToString() + "\n";
    for (const auto& tx_in : vin)
        str += "    " + tx_in.scriptWitness.ToString() + "\n";
    for (const auto& tx_out : vout)
        str += "    " + tx_out.ToString() + "\n";
    return str;
}

// check the tx's ticket vout
bool IsTicketVout(const CScript script, CScriptID &scriptID)
{
    CScriptBase::const_iterator pc = script.begin();
    opcodetype opcodeRet;
    std::vector<unsigned char> vchRet;
    if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_HASH160) {
	    vchRet.clear();
	    if (script.GetOp(pc, opcodeRet, vchRet)) {
	        scriptID = CScriptID(uint160(vchRet));
	        if (script.GetOp(pc, opcodeRet, vchRet) && opcodeRet == OP_EQUAL) {
		    vchRet.clear();
		    return true;
	        }
	    }
    }
    return false;
}

bool CTransaction::IsTicketTx() const
{
    // check the vout size is 2 or 3.
    if (IsCoinBase() || (vout.size() != 2 && vout.size() != 3)) {
	    return false;
    }
    
    CScript redeemscript;
    CScriptID scriptID;
    CScript scriptzero;
    bool HasTicketVout = false;
    for (auto i=0; i<vout.size();i++){
	    if (vout[i].nValue == 0){
	        // from 0 value vout's script decode the redeemScript.
	        auto& script = vout[i].scriptPubKey;
	        scriptzero = script;
	        if (! GetRedeemFromScript(script, redeemscript)){
                return false;
	        }
	    }

	    auto& ticketScript = vout[i].scriptPubKey;
	    if (IsTicketVout(ticketScript, scriptID)){
	        HasTicketVout=true;
	    }
    }

    if (!HasTicketVout) 
        return false;

    // parese the dest from redeemscript
    CScriptID dest = CScriptID(redeemscript);

    if (dest == scriptID){
        return true;
    }

    return false;
}

CTicketRef CTransaction::Ticket() const
{
    auto ticket = CTicketRef();
    if (!IsTicketTx()) 
        return ticket;
    CScript redeemScript;
    CScript ticketScript;
    for (int i = 0; i < vout.size(); i++) {
        auto out = vout[i];
        if (out.nValue == 0) { // op_return script
            if (!GetRedeemFromScript(out.scriptPubKey, redeemScript)) {
                //TODO: logging
                break;
            }
            ticketScript << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
        } 
    }
    for (int i = 0; i < vout.size(); i++) {
        auto out = vout[i];
        if (out.nValue != 0 && ticketScript == out.scriptPubKey) {
            ticket.reset(new CTicket(COutPoint(hash, i), out.nValue, redeemScript, ticketScript));
        }
    }
    return ticket;
}