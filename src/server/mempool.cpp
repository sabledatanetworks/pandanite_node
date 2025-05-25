#include <iostream>
#include <sstream>
#include <future>
#include <mutex>
#include <set>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include "../core/logger.hpp"
#include "../core/api.hpp"
#include "mempool.hpp"
#include "blockchain.hpp"

#define TX_BRANCH_FACTOR 10
#define MIN_FEE_TO_ENTER_MEMPOOL 1

MemPool::MemPool(HostManager &h, BlockChain &b) : hosts(h), blockchain(b)
{
    shutdown = false;
}

MemPool::~MemPool()
{
    shutdown = true;
}

void MemPool::mempool_sync() {

    const int MAX_RETRIES = 3;
    std::map<std::string, std::chrono::system_clock::time_point> failedNeighbors;
    std::mutex failedNeighborsMutex;

    while (!shutdown)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::vector<Transaction> txs;
        {
            std::unique_lock<std::mutex> lock(toSend_mutex);
            if (toSend.empty())
            {
                continue;
            }
            txs = std::vector<Transaction>(std::make_move_iterator(toSend.begin()), std::make_move_iterator(toSend.end()));
            toSend.clear();
        }

        std::vector<Transaction> invalidTxs;
        {
            std::unique_lock<std::mutex> lock(mempool_mutex);
            for (auto it = transactionQueue.begin(); it != transactionQueue.end();)
            {
                try
                {
                    ExecutionStatus status = blockchain.verifyTransaction(*it);
                    if (status != SUCCESS)
                    {
                        invalidTxs.push_back(*it);
                        it = transactionQueue.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                catch (const std::exception &e)
                {
                    std::cout << "Caught exception: " << e.what() << '\n';
                    invalidTxs.push_back(*it);
                    it = transactionQueue.erase(it);
                }
            }
        }

        if (transactionQueue.empty())
        {
            continue;
        }

        auto now = std::chrono::system_clock::now();
        for (auto it = failedNeighbors.begin(); it != failedNeighbors.end();)
        {
            if ((now - it->second) > std::chrono::hours(24))
            {
                it = failedNeighbors.erase(it);
            }
            else
            {
                ++it;
            }
        }

        std::set<std::string> neighbors = hosts.sampleFreshHosts(TX_BRANCH_FACTOR);
        std::map<std::string, int> neighborHeights; // Store block heights for neighbors

        for (const auto &neighbor : neighbors)
        {
            int height = hosts.getBlockHeightFromPeer(neighbor);
            neighborHeights[neighbor] = height;
        }

        auto maxIter = std::max_element(neighborHeights.begin(), neighborHeights.end(),
            [](const auto &lhs, const auto &rhs) {
            return lhs.second < rhs.second; });
        int maxBlockHeight = maxIter->second;

        // Filter out neighbors not at maxBlockHeight
        for(auto it = neighbors.begin(); it != neighbors.end(); ) {
            if(neighborHeights[*it] < maxBlockHeight) {
            it = neighbors.erase(it);
             } else {
                ++it;
                }
        }

        std::vector<std::future<bool>> sendResults;
        for (auto neighbor : neighbors) {
            if (failedNeighbors.find(neighbor) != failedNeighbors.end())
            {
                continue;
            }

             for (const auto& tx : txs) {
                sendResults.push_back(std::async(std::launch::async, [this, &neighbor, &tx, &failedNeighbors, &failedNeighborsMutex]() -> bool {
                for (int retry = 0; retry < MAX_RETRIES; ++retry) {
                    try {
                        sendTransaction(neighbor, tx);
                        return true;
                     } catch (...) {
                        Logger::logError("Failed to send tx to ", neighbor);
                    }
                }
                Logger::logStatus("MemPool::mempool_sync: Skipped sending to " + neighbor);
                
                // Lock the mutex only for the duration of modifying the map
                {
                    std::lock_guard<std::mutex> lock(failedNeighborsMutex);
                    failedNeighbors[neighbor] = std::chrono::system_clock::now();
                }
                
                return false;
                }));
            }
        }

            // Logging invalidTxs
            for (const auto& tx : invalidTxs) {
            Logger::logError("MemPool::mempool_sync", "A transaction is invalid and removed from the queue.");
            }

        bool all_sent = true;
        for (auto &future : sendResults)
        {
            if (!future.get())
            {
                all_sent = false;
            }
        }

        if (!all_sent)
        {
            std::unique_lock<std::mutex> lock(toSend_mutex);
            for (const auto &tx : txs)
            {
                toSend.push_back(tx);
            }
        }
    }
}

void MemPool::sync()
{
    syncThread.push_back(std::thread(&MemPool::mempool_sync, this));
    // Add cleanup thread
    cleanupThread.push_back(std::thread([this]() {
        while (true) {
            cleanupExpiredTransactions();
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
    }));
}

bool MemPool::hasTransaction(Transaction t)
{
    std::unique_lock<std::mutex> lock(mempool_mutex);
    return transactionQueue.count(t) > 0;
}

ExecutionStatus MemPool::addTransaction(Transaction t)
{
    std::unique_lock<std::mutex> lock(mempool_mutex);

    // Check if transaction is expired
    if (t.isExpired()) {
        return EXPIRED_TRANSACTION;
    }

    // Check if transaction is already in queue
    if (transactionQueue.count(t) > 0) {
        return ALREADY_IN_QUEUE;
    }

    // Check if transaction fee is sufficient
    if (t.getFee() < MIN_FEE_TO_ENTER_MEMPOOL) {
        return TRANSACTION_FEE_TOO_LOW;
    }

    // Atomic balance check and transaction verification
    if (!t.isFee()) {
        // Get a snapshot of the current state
        TransactionAmount totalRequired = t.getAmount() + t.getFee();
        TransactionAmount currentBalance = blockchain.getWalletValue(t.fromWallet());
        TransactionAmount pendingOutgoing = mempoolOutgoing[t.fromWallet()];
        
        // Verify transaction first
        ExecutionStatus status = blockchain.verifyTransaction(t);
        if (status != SUCCESS) {
            return status;
        }

        // Double check balance hasn't changed
        if (currentBalance < (totalRequired + pendingOutgoing)) {
            return BALANCE_TOO_LOW;
        }

        // Check nonce
        PublicWalletAddress from = t.fromWallet();
        uint64_t expectedNonce = walletNonces[from];
        if (t.getNonce() != expectedNonce) {
            return INVALID_NONCE;
        }

        // All checks passed, update state atomically
        mempoolOutgoing[from] += totalRequired;
        transactionQueue.insert(t);
        walletNonces[from]++;
        return SUCCESS;
    } else {
        // For fee transactions, just verify and add
        ExecutionStatus status = blockchain.verifyTransaction(t);
        if (status != SUCCESS) {
            return status;
        }
        transactionQueue.insert(t);
        return SUCCESS;
    }
}

size_t MemPool::size()
{
    std::unique_lock<std::mutex> lock(mempool_mutex);
    return transactionQueue.size();
}

std::vector<Transaction> MemPool::getTransactions() const
{
    std::unique_lock<std::mutex> lock(mempool_mutex);
    std::vector<Transaction> transactions;
    for (const auto &tx : transactionQueue)
    {
        transactions.push_back(tx);
    }
    return transactions;
}

std::pair<char *, size_t> MemPool::getRaw() const
{
    std::unique_lock<std::mutex> lock(mempool_mutex);
    size_t len = transactionQueue.size() * TRANSACTIONINFO_BUFFER_SIZE;
    char *buf = (char *)malloc(len);
    int count = 0;

    for (const auto &tx : transactionQueue)
    {
        TransactionInfo t = tx.serialize();
        transactionInfoToBuffer(t, buf + count);
        count += TRANSACTIONINFO_BUFFER_SIZE;
    }

    return std::make_pair(buf, len);
}

void MemPool::finishBlock(Block& block) {
    std::unique_lock<std::mutex> lock(mempool_mutex);
    for (const auto& tx : block.getTransactions()) {
        auto it = transactionQueue.find(tx);
        if (it != transactionQueue.end()) {
            transactionQueue.erase(it);

            if (!tx.isFee()) {
                mempoolOutgoing[tx.fromWallet()] -= tx.getAmount() + tx.getFee();

                if (mempoolOutgoing[tx.fromWallet()] == 0) {
                    mempoolOutgoing.erase(tx.fromWallet());
                }
            }
        }
    }
}

void MemPool::cleanupExpiredTransactions() {
    std::unique_lock<std::mutex> lock(mempool_mutex);
    auto now = std::chrono::system_clock::now();
    
    for (auto it = transactionQueue.begin(); it != transactionQueue.end();) {
        if (it->isExpired()) {
            if (!it->isFee()) {
                // Revert pending outgoing amount
                mempoolOutgoing[it->fromWallet()] -= (it->getAmount() + it->getFee());
            }
            it = transactionQueue.erase(it);
        } else {
            ++it;
        }
    }
}
