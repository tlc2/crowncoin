
// Copyright (c) 2014-2015 The Crown developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef SERVICENODE_H
#define SERVICENODE_H

#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "main.h"
#include "timedata.h"

#define SERVICENODE_MIN_CONFIRMATIONS           15
#define SERVICENODE_MIN_SNP_SECONDS             (10*60)
#define SERVICENODE_MIN_SNB_SECONDS             (5*60)
#define SERVICENODE_PING_SECONDS                (5*60)
#define SERVICENODE_EXPIRATION_SECONDS          (65*60)
#define SERVICENODE_REMOVAL_SECONDS             (75*60)
#define SERVICENODE_CHECK_SECONDS               5


using namespace std;

class CServicenode;
class CServicenodeBroadcast;
class CServicenodePing;
extern map<int64_t, uint256> mapCacheBlockHashes;

bool GetBlockHash(uint256& hash, int nBlockHeight);


//
// The Servicenode Ping Class : Contains a different serialize method for sending pings from servicenodes throughout the network
//

class CServicenodePing
{
public:
    CTxIn vin;
    uint256 blockHash;
    int64_t sigTime; //snb message times
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
    }
    friend bool operator==(const CServicenodePing& a, const CServicenodePing& b)
    {
        return a.vin == b.vin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CServicenodePing& a, const CServicenodePing& b)
    {
        return !(a == b);
    }
};


//
// The Servicenode Class. For managing the Darksend process. It contains the input of the 10000 CRW, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CServicenode
{
private:
    int64_t lastTimeChecked;
public:
    enum state {
        SERVICENODE_ENABLED = 1,
        SERVICENODE_EXPIRED = 2,
        SERVICENODE_VIN_SPENT = 3,
        SERVICENODE_REMOVE = 4,
        SERVICENODE_POS_ERROR = 5
    };

    CTxIn vin;
    CService addr;
    CPubKey pubkey;
    CPubKey pubkey2;
    std::vector<unsigned char> sig;
    int64_t sigTime; //snb message time
    int activeState;
    int protocolVersion;
    bool unitTest;
    CServicenodePing lastPing;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
    }
    bool IsValidNetAddr();
    bool IsEnabled()
    {
        return activeState == SERVICENODE_ENABLED;
    }
    bool IsBroadcastedWithin(int seconds)
    {
        return (GetAdjustedTime() - sigTime) < seconds;
    }
    bool UpdateFromNewBroadcast(CServicenodeBroadcast& snb);
    void Check(bool forceCheck = false);
    bool IsPingedWithin(int seconds, int64_t now = -1)
    {
        now == -1 ? now = GetAdjustedTime() : now;

        return (lastPing == CServicenodePing())
                ? false
                : now - lastPing.sigTime < seconds;
    }

};


//
// The Servicenode Broadcast Class : Contains a different serialize method for sending servicenodes through the network
//

class CServicenodeBroadcast : public CServicenode
{
public:
    bool CheckAndUpdate(int& nDoS);
    bool CheckInputsAndAdd(int& nDos);
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
    }

    uint256 GetHash(){
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << sigTime;
        ss << pubkey;
        return ss.GetHash();
    }
    void Relay();
};

#endif