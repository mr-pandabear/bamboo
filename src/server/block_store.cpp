#include <iostream>
#include <thread>
#include "../core/crypto.hpp"
#include "../core/transaction.hpp"
#include "block_store.hpp"
using namespace std;

#define BLOCK_COUNT_KEY "BLOCK_COUNT"
#define TOTAL_WORK_KEY "TOTAL_WORK"

BlockStore::BlockStore() {
}

void BlockStore::setBlockCount(size_t count) {
    string countKey = BLOCK_COUNT_KEY;
    size_t num = count;
    leveldb::Slice key = leveldb::Slice(countKey);
    leveldb::Slice slice = leveldb::Slice((const char*)&num, sizeof(size_t));
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Status status = db->Put(write_options, key, slice);
    if(!status.ok()) throw std::runtime_error("Could not write block count to DB : " + status.ToString());
}

size_t BlockStore::getBlockCount() const {
    string countKey = BLOCK_COUNT_KEY;
    leveldb::Slice key = leveldb::Slice(countKey);
    string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(),key, &value);
    if(!status.ok()) throw std::runtime_error("Could not read block count from DB : " + status.ToString());
    size_t ret = *((size_t*)value.c_str());
    return ret;
}

void BlockStore::setTotalWork(Bigint count) {
    string countKey = TOTAL_WORK_KEY;
    string sz = to_string(count);
    leveldb::Slice key = leveldb::Slice(countKey);
    leveldb::Slice slice = leveldb::Slice((const char*)sz.c_str(), sz.size());
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Status status = db->Put(write_options, key, slice);
    if(!status.ok()) throw std::runtime_error("Could not write block count to DB : " + status.ToString());
}

Bigint BlockStore::getTotalWork() const{
    string countKey = TOTAL_WORK_KEY;
    leveldb::Slice key = leveldb::Slice(countKey);
    string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(),key, &value);
    if(!status.ok()) throw std::runtime_error("Could not read block count from DB : " + status.ToString());
    if (auto v{Bigint::from_string(value)}; v.has_value()){
        return *v;
    }
    throw std::runtime_error("DB contains corrupted Bigint value for total work.");
}

bool BlockStore::hasBlockCount() {
    string countKey = BLOCK_COUNT_KEY;
    leveldb::Slice key = leveldb::Slice(countKey);
    string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(),key, &value);
    size_t ret = *((size_t*)value.c_str());
    return (status.ok());
}

bool BlockStore::hasBlock(uint32_t blockId) {
    leveldb::Slice key = leveldb::Slice((const char*) &blockId, sizeof(uint32_t));
    string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(),key, &value);
    return (status.ok());
}

BlockHeader BlockStore::getBlockHeader(uint32_t blockId) const{
    leveldb::Slice key = leveldb::Slice((const char*) &blockId, sizeof(uint32_t));
    string valueStr;
    leveldb::Status status = db->Get(leveldb::ReadOptions(),key, &valueStr);
    if(!status.ok()) throw std::runtime_error("Could not read block header " + to_string(blockId) + " from BlockStore db : " + status.ToString());
    
    BlockHeader value;
    memcpy(&value, valueStr.c_str(), sizeof(BlockHeader));
    return value;
}

vector<TransactionInfo> BlockStore::getBlockTransactions(BlockHeader& block) const{
    vector<TransactionInfo> transactions;
    for(int i = 0; i < block.numTransactions; i++) {
        int idx = i;
        int transactionId[2];
        transactionId[0] = block.id;
        transactionId[1] = idx;
        leveldb::Slice key = leveldb::Slice((const char*) transactionId, 2*sizeof(int));
        string valueStr;
        leveldb::Status status = db->Get(leveldb::ReadOptions(),key, &valueStr);
        if(!status.ok()) throw std::runtime_error("Could not read transaction from BlockStore db : " + status.ToString());
        TransactionInfo t;
        memcpy(&t, valueStr.c_str(), sizeof(TransactionInfo));
        transactions.push_back(t);
    }
    return std::move(transactions);
}

std::pair<uint8_t*, size_t> BlockStore::getRawData(uint32_t blockId) const{
    BlockHeader block = this->getBlockHeader(blockId);
    size_t numBytes = BLOCKHEADER_BUFFER_SIZE + (TRANSACTIONINFO_BUFFER_SIZE * block.numTransactions);
    char* buffer = (char*)malloc(numBytes);
    blockHeaderToBuffer(block, buffer);
    char* transactionBuffer = buffer + BLOCKHEADER_BUFFER_SIZE;
    char* currTransactionPtr = transactionBuffer;
    for(int i = 0; i < block.numTransactions; i++) {
        int idx = i;
        uint32_t transactionId[2];
        transactionId[0] = blockId;
        transactionId[1] = idx;
        leveldb::Slice key = leveldb::Slice((const char*) transactionId, 2*sizeof(uint32_t));
        string value;
        leveldb::Status status = db->Get(leveldb::ReadOptions(),key, &value);
        TransactionInfo txinfo = *(TransactionInfo*)(value.c_str());
        transactionInfoToBuffer(txinfo, currTransactionPtr);
        if (!status.ok()) throw std::runtime_error("Could not read transaction from BlockStore db : " + status.ToString());
        currTransactionPtr += TRANSACTIONINFO_BUFFER_SIZE;
    }
    return std::pair<uint8_t*, size_t>((uint8_t*)buffer, numBytes);
}

Block BlockStore::getBlock(uint32_t blockId) const{
    BlockHeader block = this->getBlockHeader(blockId);
    vector<TransactionInfo> transactionInfo = this->getBlockTransactions(block);
    vector<Transaction> transactions;
    for(auto t : transactionInfo) {
        transactions.push_back(Transaction(t));
    }
    Block ret(block, transactions);
    return ret;
}

vector<SHA256Hash> BlockStore::getTransactionsForWallet(PublicWalletAddress& wallet) const{
    struct {
        uint8_t addr[25];
        uint8_t txId[32];
    } startKey;

    struct {
        uint8_t addr[25];
        uint8_t txId[32];
    } endKey;

    memcpy(startKey.addr,wallet.data(), 25);
    memset(startKey.txId, 0, 32);
    memcpy(endKey.addr, wallet.data(), 25);
    memset(endKey.txId, -1, 32);

    auto startSlice = leveldb::Slice((const char*) &startKey, sizeof(startKey));
    auto endSlice = leveldb::Slice((const char*) &endKey, sizeof(endKey));

    // Get a database iterator
    std::shared_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));

    vector<SHA256Hash> ret;
    for(it->Seek(startSlice); it->Valid() && it->key().compare(endSlice) < 0; it->Next()) {                
        leveldb::Slice keySlice(it->key());
        SHA256Hash txid;
        memcpy(txid.data(), keySlice.data() + 25, 32);
        ret.push_back(txid);
    }
    return std::move(ret);
}


void BlockStore::removeBlockWalletTransactions(Block& block) {
    for(auto t : block.getTransactions()) {
        SHA256Hash txid = t.hashContents();
        struct {
            uint8_t addr[25];
            uint8_t txId[32];
        } w1Key;

        struct {
            uint8_t addr[25];
            uint8_t txId[32];
        } w2Key;

        memcpy(w1Key.addr, t.fromWallet().data(), 25);
        memcpy(w1Key.txId, txid.data(), 32);
        memcpy(w2Key.addr, t.toWallet().data(), 25);
        memcpy(w2Key.txId, txid.data(), 32);

        leveldb::Slice key = leveldb::Slice((const char*) &w1Key, sizeof(w1Key));
        leveldb::Status status = db->Delete(leveldb::WriteOptions(), key);
        if(!status.ok()) throw std::runtime_error("Could not remove transaction from wallet in blockstore db : " + status.ToString());

        key = leveldb::Slice((const char*) &w2Key, sizeof(w2Key));
        status = db->Delete(leveldb::WriteOptions(), key);
        if(!status.ok()) throw std::runtime_error("Could not remove transaction from wallet in blockstore db : " + status.ToString());
    }
}

void BlockStore::setBlock(Block& block) {
    uint32_t blockId = block.getId();
    leveldb::Slice key = leveldb::Slice((const char*) &blockId, sizeof(uint32_t));
    BlockHeader blockStruct = block.serialize();
    leveldb::Slice slice = leveldb::Slice((const char*)&blockStruct, sizeof(BlockHeader));
    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, slice);
    if(!status.ok()) throw std::runtime_error("Could not write block to BlockStore db : " + status.ToString());
    for(int i = 0; i < block.getTransactions().size(); i++) {
        uint32_t transactionId[2];
        transactionId[0] = blockId;
        transactionId[1] = i;
        TransactionInfo t = block.getTransactions()[i].serialize();
        leveldb::Slice key = leveldb::Slice((const char*) transactionId, 2*sizeof(uint32_t));
        leveldb::Slice slice = leveldb::Slice((const char*)&t, sizeof(TransactionInfo));
        leveldb::Status status = db->Put(leveldb::WriteOptions(), key, slice);
        if(!status.ok()) throw std::runtime_error("Could not write transaction to BlockStore db : " + status.ToString());

        // add the transaction to from and to wallets list of transactions
        SHA256Hash txid = block.getTransactions()[i].hashContents();
        struct {
            uint8_t addr[25];
            uint8_t txId[32];
        } w1Key;

        struct {
            uint8_t addr[25];
            uint8_t txId[32];
        } w2Key;

        memcpy(w1Key.addr, t.from.data(), 25);
        memcpy(w1Key.txId, txid.data(), 32);
        memcpy(w2Key.addr, t.to.data(), 25);
        memcpy(w2Key.txId, txid.data(), 32);

        key = leveldb::Slice((const char*) &w1Key, sizeof(w1Key));
        slice = leveldb::Slice("", 0);
        status = db->Put(leveldb::WriteOptions(), key, slice);
        if(!status.ok()) throw std::runtime_error("Could not write wallet transactions to BlockStore db : " + status.ToString());

        key = leveldb::Slice((const char*) &w2Key, sizeof(w2Key));
        slice = leveldb::Slice("", 0);
        status = db->Put(leveldb::WriteOptions(), key, slice);
        if(!status.ok()) throw std::runtime_error("Could not write wallet transactions to BlockStore db : " + status.ToString());
    }
}

