#include "transaction.hpp"
#include "helpers.hpp"
#include "block.hpp"
#include "openssl/sha.h"
#include <sstream>
#include <iostream>
 #include <cstring>
#include <ctime>
using namespace std;


TransactionInfo transactionInfoFromBuffer(const char* buffer) {
    TransactionInfo t;
    readNetworkNBytes(buffer, t.signature, 64);
    readNetworkNBytes(buffer, t.signingKey, 32);
    t.timestamp = readNetworkUint64(buffer);
    t.to = readNetworkPublicWalletAddress(buffer);
    t.amount = readNetworkUint64(buffer);
    t.fee = readNetworkUint64(buffer);
    t.isTransactionFee = readNetworkUint32(buffer) > 0;

    if (!t.isTransactionFee) {
        PublicKey pub;
        memcpy(pub.data(), t.signingKey, 32);
        t.from = walletAddressFromPublicKey(pub);
    } else {
        t.from = NULL_ADDRESS;
    }
    t.programId = readNetworkSHA256(buffer);
    readNetworkNBytes(buffer, (char*)t.data.data(), 128);
    return t;
}

void transactionInfoToBuffer(TransactionInfo& t, char* buffer) {
    writeNetworkNBytes(buffer, t.signature, 64);
    writeNetworkNBytes(buffer, t.signingKey, 32);
    writeNetworkUint64(buffer, t.timestamp);
    writeNetworkPublicWalletAddress(buffer, t.to);
    writeNetworkUint64(buffer, t.amount);
    writeNetworkUint64(buffer, t.fee);
    uint32_t flag = 0;
    if (t.isTransactionFee) flag = 1;
    writeNetworkUint32(buffer, flag);
    writeNetworkSHA256(buffer,t.programId);
    writeNetworkNBytes(buffer, (char*)t.data.data(), 128);
}

Transaction::Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee) {
    this->from = from;
    this->to = to;
    this->amount = amount;
    this->isTransactionFee = false;
    this->timestamp = std::time(0);
    this->fee = fee;
    this->signingKey = signingKey;
    this->data.fill(0);
    this->programId = NULL_SHA256_HASH;
}

Transaction::Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee, uint64_t timestamp) {
    this->from = from;
    this->to = to;
    this->amount = amount;
    this->isTransactionFee = false;
    this->timestamp = timestamp;
    this->fee = fee;
    this->signingKey = signingKey;
    this->data.fill(0);
    this->programId = NULL_SHA256_HASH;
}

Transaction::Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee, ProgramID programId, TransactionData data) {
    this->from = from;
    this->to = to;
    this->amount = amount;
    this->isTransactionFee = false;
    this->timestamp = LAYER_2_TX_FLAG;
    this->fee = fee;
    this->signingKey = signingKey;
    this->data = data;
    this->programId = programId;
}

Transaction::Transaction(PublicWalletAddress from, PublicKey signingKey, TransactionAmount fee, ProgramID programId) {
    this->from = from;
    this->to = NULL_ADDRESS;
    this->amount = 0;
    this->isTransactionFee = false;
    this->timestamp = PROGRAM_CREATE_TX_FLAG;
    this->fee = fee;
    this->signingKey = signingKey;
    this->data.fill(0);
    this->programId = programId;
}

Transaction::Transaction() {

}

Transaction::Transaction(const TransactionInfo& t) {
    this->to = t.to;
    if (!t.isTransactionFee) this->from = t.from;
    memcpy((void*)this->signature.data(), (void*)t.signature, 64);
    memcpy((void*)this->signingKey.data(), (void*)t.signingKey, 32);
    this->amount = t.amount;
    this->isTransactionFee = t.isTransactionFee;
    this->timestamp = t.timestamp;
    this->fee = t.fee;
    this->data = t.data;
    this->programId = t.programId;
}
TransactionInfo Transaction::serialize() {
    TransactionInfo t;
    memcpy((void*)t.signature, (void*)this->signature.data(), 64);
    memcpy((void*)t.signingKey, (void*)this->signingKey.data(), 32);
    t.timestamp = this->timestamp;
    t.to = this->to;
    t.from = this->from;
    t.amount = this->amount;
    t.fee = this->fee;
    t.isTransactionFee = this->isTransactionFee;
    t.data = this->data;
    t.programId = this->programId;
    return t;
}

bool Transaction::isLayer2() const {
    return this->timestamp == LAYER_2_TX_FLAG;
}

bool Transaction::isProgramExecution() const {
    return this->timestamp == PROGRAM_CREATE_TX_FLAG;
}

ProgramID Transaction::getProgramId() const {
    if (!this->isLayer2() && !this->isProgramExecution()) throw std::runtime_error("Cannot get programID of transaction");
    return this->programId;
}

TransactionData Transaction::getData() const {
    if (!this->isLayer2()) throw std::runtime_error("Cannot get tx data of non-layer 2 transaction");
    return this->data;
}

Transaction::Transaction(const Transaction & t) {
    this->to = t.to;
    this->from = t.from;
    this->signature = t.signature;
    this->amount = t.amount;
    this->isTransactionFee = t.isTransactionFee;
    this->timestamp = t.timestamp;
    this->fee = t.fee;
    this->signingKey = t.signingKey;
    this->programId = t.programId;
    this->data = t.data;
}

Transaction::Transaction(PublicWalletAddress to, TransactionAmount fee) {
    this->to = to;
    this->amount = fee;
    this->isTransactionFee = true;
    this->timestamp = getCurrentTime();
    this->fee = 0;
}

Transaction::Transaction(json obj) {
    PublicWalletAddress to;
    this->timestamp = stringToUint64(obj["timestamp"]);

    if (this->timestamp == LAYER_2_TX_FLAG) {
        this->programId = stringToSHA256(obj["programId"]);
        vector<uint8_t> bytes = hexDecode(obj["data"]);
        memcpy(this->data.data(), bytes.data(), this->data.size());
    } else if (this->timestamp == PROGRAM_CREATE_TX_FLAG) {
        this->programId = stringToSHA256(obj["programId"]);
        this->data.fill(0);
    } else {
        this->programId = NULL_SHA256_HASH;
        this->data.fill(0);
    }

    this->to = stringToWalletAddress(obj["to"]);
    this->fee = obj["fee"];
    if(obj["from"] == "") {        
        this->amount = obj["amount"];
        this->isTransactionFee = true;
    } else {
        this->from = stringToWalletAddress(obj["from"]);
        this->signature = stringToSignature(obj["signature"]);
        this->amount = obj["amount"];
        this->isTransactionFee = false;
        this->signingKey = stringToPublicKey(obj["signingKey"]);
    }
}

void Transaction::setTransactionFee(TransactionAmount amount) {
    this->fee = amount;
}
TransactionAmount Transaction::getTransactionFee() const {
    return this->fee;
}

void Transaction::makeLayer2(ProgramID programId, TransactionData data) {
    this->timestamp = LAYER_2_TX_FLAG;
    this->programId = programId;
    this->data = data;
}

json Transaction::toJson() {
    json result;
    result["to"] = walletAddressToString(this->toWallet());
    result["amount"] = this->amount;
    result["timestamp"] = uint64ToString(this->timestamp);
    result["fee"] = this->fee;
    if (this->isLayer2()) {
        result["data"] = hexEncode((const char*)this->data.data(), this->data.size());
        result["programId"] = SHA256toString(this->programId);
    }
    if (this->isProgramExecution()) {
        result["programId"] = SHA256toString(this->programId);
    }
    if (!this->isTransactionFee) {
        result["txid"] = SHA256toString(this->hashContents());
        result["from"] = walletAddressToString(this->fromWallet());
        result["signingKey"] = publicKeyToString(this->signingKey);
        result["signature"] = signatureToString(this->signature);
    } else {
        result["txid"] = SHA256toString(this->hashContents());
        result["from"] = "";
    }
    
    return result;
}


bool Transaction::isFee() const {
    return this->isTransactionFee;
}

void Transaction::setTimestamp(uint64_t t) {
    this->timestamp = t;
}

uint64_t Transaction::getNonce() {
    return this->timestamp;
}

TransactionSignature Transaction::getSignature() const {
    return this->signature;
}

void Transaction::setAmount(TransactionAmount amt) {
    this->amount = amt;
}

bool Transaction::signatureValid() const {
    if (this->isFee()) return true;
    SHA256Hash hash = this->hashContents();
    return checkSignature((const char*)hash.data(), hash.size(), this->signature, this->signingKey);

}

PublicKey Transaction::getSigningKey() {
    return this->signingKey;
}


SHA256Hash Transaction::getHash() const {
    SHA256Hash ret;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256Hash contentHash = this->hashContents();
    SHA256_Update(&sha256, (unsigned char*)contentHash.data(), contentHash.size());
    if (!this->isTransactionFee) {
        SHA256_Update(&sha256, (unsigned char*)this->signature.data(), this->signature.size());
    }
    SHA256_Final(ret.data(), &sha256);
    return ret;
}

PublicWalletAddress Transaction::fromWallet() const {
    return this->from;
}
PublicWalletAddress Transaction::toWallet() const {
    return this->to;
}

TransactionAmount Transaction::getAmount() const {
    return this->amount;
}

TransactionAmount Transaction::getFee() const {
    return this->fee;
}

SHA256Hash Transaction::hashContents() const {
    SHA256Hash ret;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    PublicWalletAddress wallet = this->toWallet();
    SHA256_Update(&sha256, (unsigned char*)wallet.data(), wallet.size());
    if (!this->isTransactionFee) {
        wallet = this->fromWallet();
        SHA256_Update(&sha256, (unsigned char*)wallet.data(), wallet.size());
    }
    if (this->isProgramExecution()) {
        SHA256_Update(&sha256, (unsigned char*)this->getProgramId().data(), this->getProgramId().size());
    }
    SHA256_Update(&sha256, (unsigned char*)&this->fee, sizeof(TransactionAmount));
    SHA256_Update(&sha256, (unsigned char*)&this->amount, sizeof(TransactionAmount));
    SHA256_Update(&sha256, (unsigned char*)&this->timestamp, sizeof(uint64_t));
    SHA256_Final(ret.data(), &sha256);
    return ret;
}

void Transaction::sign(PublicKey pubKey, PrivateKey signingKey) {
    SHA256Hash hash = this->hashContents();
    TransactionSignature signature = signWithPrivateKey((const char*)hash.data(), hash.size(), pubKey, signingKey);
    this->signature = signature;
}

bool operator<(const Transaction& a, const Transaction& b) {
    return a.signature < b.signature;
}

bool operator==(const Transaction& a, const Transaction& b) {
    if(a.timestamp != b.timestamp) return false;
    if(a.toWallet() != b.toWallet()) return false;
    if(a.getTransactionFee() != b.getTransactionFee()) return false;
    if(a.amount != b.amount) return false;
    if(a.isTransactionFee != b.isTransactionFee) return false;
    if(a.isLayer2()) {
        if (a.programId != b.programId) return false;
        if (a.data != b.data) return false;
    }
    if(a.isProgramExecution()) {
        if (a.programId != b.programId) return false;
    }
    if (!a.isTransactionFee) {
        if( a.fromWallet() != b.fromWallet()) return false;
        if(signatureToString(a.signature) != signatureToString(b.signature)) return false;
        if (publicKeyToString(a.signingKey) != publicKeyToString(b.signingKey)) return false;
    }
    return true;
}