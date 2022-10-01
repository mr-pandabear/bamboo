#include <emscripten/emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include "../core/transaction.hpp"
#include "../core/block.hpp"
#include "../core/crypto.hpp"
#include "../core/types.hpp"
#include "../external/pufferfish/pufferfish.hpp"
#include "../external/ed25519/ed25519.h"
#include "../external/json.hpp"
#include "openssl/hmac.h"
using namespace std;


#ifdef __cplusplus
extern "C" {
#endif
    #include "external_calls.hpp"

    void executeBlock(char* args) {
        char* ptr = args;
        print("starting");
        BlockHeader blockH = blockHeaderFromBuffer(ptr);
        ExecutionStatus e = SUCCESS;
        ptr += BLOCKHEADER_BUFFER_SIZE;
        vector<Transaction> transactions;
        for(int i = 0; i <  blockH.numTransactions; i++) {
            TransactionInfo t = transactionInfoFromBuffer(ptr, false);
            Transaction tx(t);
            transactions.push_back(tx);
            ptr += transactionInfoBufferSize(false);
        }
        print("read block");
        Block block(blockH, transactions);
        char str[1000];
        sprintf(str, "block ID %d", block.getId());
        print(str);
        if (block.getId() == 1) {
            print("genesis");
            // generate the genesis record of NFT owner
            PublicWalletAddress ownerAddress = NULL_ADDRESS;
            SHA256Hash contentHash = NULL_SHA256_HASH;
            setWallet("owner", ownerAddress);
            setSha256("content", contentHash);
            e = SUCCESS;
            setReturnValue((char*)&e, sizeof(ExecutionStatus));
            return;
        } else {
            print("other");
            for(auto tx : block.getTransactions()) {
                PublicWalletAddress sender = tx.fromWallet();
                PublicWalletAddress recepient = tx.toWallet();
                PublicWalletAddress owner = getWallet("owner");
                if (owner == sender) {
                    //check the signature of the signing key
                    print("checking");
                    if (true) {
                        e = INVALID_SIGNATURE;
                        setReturnValue((char*)&e, sizeof(ExecutionStatus));
                        print("checking3");
                        return;
                    }
                    setWallet("owner", recepient);
                } else {
                    print("checking2");
                    e = BALANCE_TOO_LOW;
                    setReturnValue((char*)&e, sizeof(ExecutionStatus));
                    return;
                }
            }
        }
    }

#ifdef __cplusplus
}
#endif
