#pragma once

#include <set>
#include <map>
#include <thread>
#include <mutex>
#include <list>
#include <map>
#include "../core/host_manager.hpp"
#include "../core/transaction.hpp"
#include "executor.hpp"
#include "../core/block.hpp"
#include "../core/common.hpp"

class BlockChain;

class MemPool {
public:
    MemPool(HostManager& h, BlockChain& b);
    ~MemPool();
    void sync();
    ExecutionStatus addTransaction(Transaction t);
    void finishBlock(Block& block);
    bool hasTransaction(Transaction t);
    size_t size();
    std::pair<char*, size_t> getRaw() const;
    std::vector<Transaction> getTransactions() const;
    void removeTransaction(Transaction t);
    void cleanupExpiredTransactions();

protected:
    void mempool_sync();
    bool shutdown;
    std::mutex shutdownLock;
    std::map<PublicWalletAddress, TransactionAmount> mempoolOutgoing;
    std::list<Transaction> toSend;
    BlockChain& blockchain;
    HostManager& hosts;
    
    // Transaction ordering by fee
    struct TransactionComparator {
        bool operator()(const Transaction& a, const Transaction& b) const {
            if (a.getFee() != b.getFee()) {
                return a.getFee() > b.getFee();
            }
            return a.getHash() < b.getHash();
        }
    };
    
    std::set<Transaction, TransactionComparator> transactionQueue;
    std::vector<std::thread> syncThread;
    mutable std::mutex mempool_mutex;
    std::mutex toSend_mutex;
    
    // Track nonces for each wallet
    std::map<PublicWalletAddress, uint64_t> walletNonces;
    std::map<SHA256Hash, Transaction> transactions;
    std::vector<std::thread> cleanupThread;
    mutable std::mutex lock;
};
