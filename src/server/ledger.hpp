#pragma once
#include "../core/common.hpp"
#include <leveldb/db.h>
#include <mutex>
#include <memory>

class Ledger {
    public:
        Ledger();
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
        
        // New atomic operations
        bool atomicWithdrawIfSufficient(const PublicWalletAddress& wallet, TransactionAmount amt);
        bool atomicDepositIfValid(const PublicWalletAddress& wallet, TransactionAmount amt);
        
        // New methods for state management
        void clear();
        LedgerState getState() const;
        
    protected:
        std::unique_ptr<leveldb::DB> db;
        std::mutex ledger_mutex;
};
