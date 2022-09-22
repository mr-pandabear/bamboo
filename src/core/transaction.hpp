#pragma once
#include "constants.hpp"
#include "common.hpp"
#include "crypto.hpp"
#include <optional>
using namespace std;

#define LAYER_2_TX_FLAG -1
#define PROGRAM_CREATE_TX_FLAG -2

#define TRANSACTIONINFO_BUFFER_SIZE 309
#define TRANSACTIONINFO_BUFFER_SIZE_V0 149

TransactionInfo transactionInfoFromBuffer(const char* buffer, bool useV0=true);
void transactionInfoToBuffer(TransactionInfo& t, char* buffer, bool useV0 = true);
uint64_t transactionInfoBufferSize(bool useV0 = true);

class Transaction {
    public:
        Transaction(json t);
        Transaction();
        Transaction(const Transaction & t);
        Transaction(PublicWalletAddress to, TransactionAmount fee);
        Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee=0);
        Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee, uint64_t timestamp);
        Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee, ProgramID programId, TransactionData data);
        Transaction(PublicWalletAddress from, PublicKey signingKey, TransactionAmount fee, ProgramID programId);
        Transaction(const TransactionInfo& t);
        TransactionInfo serialize();
        json toJson();
        void sign(PublicKey pubKey, PrivateKey signingKey);
        void setTransactionFee(TransactionAmount amount);
        void makeLayer2(ProgramID programId, TransactionData data);
        TransactionAmount getTransactionFee() const;
        void setAmount(TransactionAmount amt);
        PublicKey getSigningKey() const;
        PublicWalletAddress fromWallet() const;
        PublicWalletAddress toWallet() const;
        TransactionAmount getAmount() const;
        TransactionAmount getFee() const;
        void setNonce(uint64_t t);
        uint64_t getNonce();
        SHA256Hash getHash() const;
        SHA256Hash hashContents() const;
        TransactionSignature getSignature() const;
        bool signatureValid() const;
        bool isFee() const;
        bool isLayer2() const;
        bool isProgramExecution() const;
        ProgramID getProgramId() const;
        TransactionData getData() const;
        static Transaction createGenesisTransaction(PublicWalletAddress to, TransactionAmount amount);
    protected:
        TransactionSignature signature;
        PublicKey signingKey;
        uint64_t nonce;
        ProgramID programId;
        TransactionData data;
        PublicWalletAddress to;
        PublicWalletAddress from;
        TransactionAmount amount;
        TransactionAmount fee;
        bool isTransactionFee;
        friend bool operator==(const Transaction& a, const Transaction& b);
        friend bool operator<(const Transaction& a, const Transaction& b);
};

bool operator==(const Transaction& a, const Transaction& b);

