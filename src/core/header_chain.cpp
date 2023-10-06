#include "header_chain.hpp"
#include "api.hpp"
#include "block.hpp"
#include "spdlog/spdlog.h"
#include <iostream>
#include <thread>
using namespace std;

#define MAX_HASH_RETENTION 1000

void chain_sync(HeaderChain& chain) {
    while(true) {
        if (!chain.triedBlockStoreCache && chain.blockStore) {
            uint64_t chainLength = chain.blockStore->getBlockCount();
            for(uint64_t i = 1; i <= chainLength; i++) {
                chain.blockHashes.push_back(chain.blockStore->getBlock(i).getHash());
            }
            chain.totalWork = chain.blockStore->getTotalWork();
            chain.chainLength = chainLength;
            chain.triedBlockStoreCache = true;
            chain.load();
        } else {
            chain.load();
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}


HeaderChain::HeaderChain(string host, map<uint64_t, SHA256Hash>& checkpoints, map<uint64_t, SHA256Hash>& bannedHashes, std::shared_ptr<BlockStore> blockStore) {
    this->host = host;
    this->failed = false;
    this->offset = 0;
    this->totalWork = 0;
    this->chainLength = 0;
    this->blockStore = blockStore;
    this->triedBlockStoreCache = false;
    this->syncThread.push_back(std::thread(chain_sync, ref(*this)));
    this->checkPoints = checkpoints;
    this->bannedHashes = bannedHashes;
}

SHA256Hash HeaderChain::getHash(uint64_t blockId) const{
    if (blockId >= this->blockHashes.size()) return NULL_SHA256_HASH;
    return this->blockHashes[blockId - 1];
}

void HeaderChain::reset() {
    this->failed = false;
    this->offset = 0;
    this->totalWork = 0;
    this->chainLength = 0;
    this->blockHashes.clear();
}

bool HeaderChain::valid() {
    return !this->failed && this->totalWork > 0;
}

string HeaderChain::getHost() const{
    return this->host;
}

Bigint HeaderChain::getTotalWork() const{
    if (this->failed) return 0;
    return this->totalWork;
}
uint64_t HeaderChain::getChainLength() const{
    if (this->failed) return 0;
    return this->chainLength;
}

uint64_t HeaderChain::getCurrentDownloaded() const{
    return this->blockHashes.size();
}

void HeaderChain::load() {
    auto opt{ getCurrentBlockCount(this->host)};
    if (!opt.has_value()) {
        this->failed = true;
        return;
    }
    uint64_t targetBlockCount{*opt};
    
    SHA256Hash lastHash = NULL_SHA256_HASH;
    if (this->blockHashes.size() > 0) {
        lastHash = this->blockHashes.back();
    }
    uint64_t numBlocks = this->blockHashes.size();
    uint64_t startBlocks = numBlocks;
    Bigint totalWork = this->totalWork;
    // download any remaining blocks in batches
    for(int i = numBlocks + 1; i <= targetBlockCount; i+=BLOCK_HEADERS_PER_FETCH) {
        try {
            int end = min(targetBlockCount, (uint64_t) i + BLOCK_HEADERS_PER_FETCH - 1);
            bool failure = false;
            vector<BlockHeader> blockHeaders;
            readRawHeaders(this->host, i, end, blockHeaders);
            for (auto& b : blockHeaders) {
                vector<Transaction> empty;
                Block block(b, empty);
                uint64_t curr = b.id;
                if (this->bannedHashes.find(curr) != this->bannedHashes.end()) {
                    // check if the current hash corresponds to a banned hash
                    if (block.getHash() == this->bannedHashes[curr]) {
                        spdlog::info("Banned hash found for block: {}", to_string(curr));
                        failure = true;
                        break;
                    } 
                }
                if (this->checkPoints.find(curr) != this->checkPoints.end()) {
                    // check the checkpoint hash:
                    if (block.getHash() != this->checkPoints[curr]) {
                        // spdlog::info("Checkpoint hash failed for block: {}", to_string(curr));
                        failure = true;
                        break;
                    } 
                }
                if (!block.verifyNonce()) {
                    failure = true;
                    break;
                };
                if (block.getLastBlockHash() != lastHash) {
                    failure = true;
                    break;
                }
                lastHash = block.getHash();
                this->blockHashes.push_back(lastHash);
                totalWork = addWork(totalWork, block.getDifficulty());
                numBlocks++;
                this->chainLength = numBlocks;
                this->totalWork = totalWork;
            }
            if (failure) {
                spdlog::warn("header chain sync failed host={}", this->host);
                this->failed = true;
                this->reset();
                return;
            }
        } catch (std::exception& e) {
            this->failed = true;
            this->reset();
            return;
        } catch (...) {
            this->failed = true;
            this->reset();
            return;
        }
    }
    this->failed = false;
    if (numBlocks != startBlocks) {
        spdlog::info("Chain for {} updated to length= {}  total_work= {}" , this->host, to_string(this->chainLength), to_string(this->totalWork));
    }
}


