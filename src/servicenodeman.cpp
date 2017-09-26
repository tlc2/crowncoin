// Copyright (c) 2014-2015 The Crown developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "servicenodeman.h"
#include "darksend.h"
#include "util.h"
#include "addrman.h"
#include "spork.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

/** Servicenode manager */
CServicenodeMan snodeman;

void CServicenodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; //disable all Darksend/Servicenode related functionality
    //if(!servicenodeSync.IsBlockchainSynced()) return;

    LOCK(cs_process_message);

    if (strCommand == "snb") { //Servicenode Broadcast
        CServicenodeBroadcast snb;
        vRecv >> snb;

        int nDoS = 0;
        if (CheckSnbAndUpdateServicenodeList(snb, nDoS)) {
            // use announced Servicenode as a peer
             addrman.Add(CAddress(snb.addr), pfrom->addr, 2*60*60);
        } else {
            if(nDoS > 0) Misbehaving(pfrom->GetId(), nDoS);
        }
    }
}

bool CServicenodeMan::CheckSnbAndUpdateServicenodeList(CServicenodeBroadcast snb, int& nDos)
{
    nDos = 0;
    LogPrint("servicenode", "CServicenodeMan::CheckSnbAndUpdateServicenodeList - Servicenode broadcast, vin: %s\n", snb.vin.ToString());

    if(mapSeenServicenodeBroadcast.count(snb.GetHash())) { //seen
//        servicenodeSync.AddedServicenodeList(snb.GetHash());
        return true;
    }
    mapSeenServicenodeBroadcast.insert(make_pair(snb.GetHash(), snb));

    LogPrint("servicenode", "CServicenodeMan::CheckSnbAndUpdateServicenodeList - Servicenode broadcast, vin: %s new\n", snb.vin.ToString());
    // We check addr before both initial snb and update
    if(!snb.IsValidNetAddr()) {
        LogPrintf("CMasternodeBroadcast::CheckSnbAndUpdateMasternodeList -- Invalid addr, rejected: masternode=%s  sigTime=%lld  addr=%s\n",
                    snb.vin.prevout.ToStringShort(), snb.sigTime, snb.addr.ToString());
        return false;
    }

    if(!snb.CheckAndUpdate(nDos)) {
        LogPrint("servicenode", "CServicenodeMan::CheckSnbAndUpdateServicenodeList - Servicenode broadcast, vin: %s CheckAndUpdate failed\n", snb.vin.ToString());
        return false;
    }

    // make sure the vout that was signed is related to the transaction that spawned the Servicenode
    //  - this is expensive, so it's only done once per Servicenode
    if(!darkSendSigner.IsVinAssociatedWithPubkey(snb.vin, snb.pubkey)) {
        LogPrintf("CServicenodeMan::CheckSnbAndUpdateServicenodeList - Got mismatched pubkey and vin\n");
        nDos = 33;
        return false;
    }

    // make sure it's still unspent
    //  - this is checked later by .check() in many places and by ThreadCheckDarkSendPool()
    if(snb.CheckInputsAndAdd(nDos)) {
//        servicenodeSync.AddedServicenodeList(snb.GetHash());
    } else {
        LogPrintf("CServicenodeMan::CheckSnbAndUpdateServicenodeList - Rejected Servicenode entry %s\n", snb.addr.ToString());
        return false;
    }

    return true;
}

CServicenode *CServicenodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CServicenode& sn, vServicenodes)
    {
        if(sn.vin.prevout == vin.prevout)
            return &sn;
    }
    return NULL;
}

bool CServicenodeMan::Add(CServicenode &sn)
{
    LOCK(cs);

    if (!sn.IsEnabled())
        return false;

    CServicenode *psn = Find(sn.vin);
    if (psn == NULL)
    {
        LogPrint("throne", "CServicenodeMan: Adding new Servicenode %s - %i now\n", sn.addr.ToString(), size() + 1);
        vServicenodes.push_back(sn);
        return true;
    }

    return false;
}