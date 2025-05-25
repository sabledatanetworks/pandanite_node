#pragma once
#include <list>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "../core/block.hpp"
#include "../core/api.hpp"
#include "../core/constants.hpp"
#include "../core/common.hpp"
#include "../core/host_manager.hpp"
#include "executor.hpp"
#include "block_store.hpp"
#include "ledger.hpp"
#include "tx_store.hpp"
using namespace std;

class MemPool;

class BlockChain {
    public:
        BlockChain(HostManager& hosts, string ledgerPath="", string blockPath="", string txdbPath="");
        ~BlockChain();
        void sync();
        Block getBlock(uint32_t blockId) const;
        Bigint getTotalWork() const ;
        uint8_t getDifficulty() const;
        uint32_t getBlockCount() const;
        uint32_t getCurrentMiningFee(uint64_t blockId) const;
        double getSupply() const; // locks mutex
        SHA256Hash getLastHash() const;
        const Ledger& getLedger() const;
        Ledger& getLedger();
        uint32_t findBlockForTransaction(Transaction &t);
        uint32_t findBlockForTransactionId(SHA256Hash txid);
        ExecutionStatus addBlockSync(Block& block);
        ExecutionStatus verifyTransaction(const Transaction& t);
        std::pair<uint8_t*, size_t> getRaw(uint32_t blockId) const;
        BlockHeader getBlockHeader(uint32_t blockId) const;
        TransactionAmount getWalletValue(PublicWalletAddress addr) const;
        uint64_t getWalletNonce(const PublicWalletAddress& wallet) const;
        map<string, uint64_t> getHeaderChainStats() const;
        vector<Transaction> getTransactionsForWallet(PublicWalletAddress addr) const;
        void setMemPool(std::shared_ptr<MemPool> memPool);
        void initChain();
        void recomputeLedger();
        void resetChain();
        void popBlock();
        void deleteDB();
        void closeDB();
        ExecutionStatus addBlock(Block& block);
    protected:
        bool isSyncing;
        bool shutdown;
        HostManager& hosts;
        std::shared_ptr<MemPool> memPool;
        int numBlocks;
        int retries;
        Bigint totalWork;
        std::shared_ptr<BlockStore> blockStore;
        Ledger ledger;
        TransactionStore txdb;
        SHA256Hash lastHash;
        int difficulty;
        void updateDifficulty();
        ExecutionStatus startChainSync();
        int targetBlockCount;
        mutable std::mutex lock;
        vector<std::thread> syncThread;
        map<int,SHA256Hash> checkpoints;
        friend void chain_sync(BlockChain& blockchain);
};
