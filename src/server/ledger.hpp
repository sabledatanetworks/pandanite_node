#pragma once
#include "../core/common.hpp"
#include <leveldb/db.h>
#include <mutex>
#include <memory>
#include <map>
#include "../core/transaction.hpp"
#include "ledger_state.hpp"

class Ledger {
    public:
        Ledger();
        ~Ledger();
        void init(const std::string& dbPath);
        void closeDB();
        void deleteDB();
        bool hasWallet(const PublicWalletAddress& wallet) const;
        void createWallet(const PublicWalletAddress& wallet);
        void setWalletValue(const PublicWalletAddress& wallet, TransactionAmount amount);
        TransactionAmount getWalletValue(const PublicWalletAddress& wallet) const;
        void withdraw(const PublicWalletAddress& wallet, TransactionAmount amt);
        void deposit(const PublicWalletAddress& wallet, TransactionAmount amt);
        void revertSend(const PublicWalletAddress& wallet, TransactionAmount amt);
        void revertDeposit(PublicWalletAddress to, TransactionAmount amt);
        
        // Atomic operations
        bool atomicWithdrawIfSufficient(const PublicWalletAddress& wallet, TransactionAmount amt);
        bool atomicDepositIfValid(const PublicWalletAddress& wallet, TransactionAmount amt);
        
        // State management
        void clear();
        LedgerState getState() const;
        uint64_t getWalletNonce(const PublicWalletAddress& wallet) const;
        void incrementWalletNonce(const PublicWalletAddress& wallet);
        
    protected:
        std::unique_ptr<leveldb::DB> db;
        std::string dbPath;
        mutable std::mutex ledger_mutex;
        std::map<PublicWalletAddress, TransactionAmount> balances;
        std::map<PublicWalletAddress, uint64_t> nonces;
        mutable std::mutex lock;
};
