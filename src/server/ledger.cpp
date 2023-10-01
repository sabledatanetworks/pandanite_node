#include <iostream>
#include <thread>
#include "../core/crypto.hpp"
#include "ledger.hpp"
#include "../core/logger.hpp"
using namespace std;

const size_t expectedWalletSize = sizeof(PublicWalletAddress);
const size_t expectedAmountSize = sizeof(TransactionAmount);

Ledger::Ledger() {
}

leveldb::Slice walletToSlice(const PublicWalletAddress& w) {
    if (w.size() != expectedWalletSize) {
      //  Logger::logError("Data integrity error: Mismatched wallet data size during write. Expected: ", 
        //            std::to_string(expectedWalletSize) + ", Got: " + std::to_string(w.size()));
        //throw std::runtime_error("Data integrity error");
    }
    leveldb::Slice s2 = leveldb::Slice((const char*) w.data(), w.size());
    return s2;
}

leveldb::Slice amountToSlice(const TransactionAmount& t) {
    leveldb::Slice s2 = leveldb::Slice((const char*) &t, sizeof(TransactionAmount));
    return s2;
}

bool Ledger::hasWallet(const PublicWalletAddress& wallet) const {
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), walletToSlice(wallet), &value);
    return (status.ok());
}

void Ledger::createWallet(const PublicWalletAddress& wallet) {
    if (this->hasWallet(wallet)) throw std::runtime_error("Wallet exists");
    this->setWalletValue(wallet, 0);
}

void Ledger::setWalletValue(const PublicWalletAddress& wallet, TransactionAmount amount) {
//    Logger::logError("Setting value for wallet: ", walletAddressToString(wallet) + ", Amount: " + std::to_string(amount));
    leveldb::Status status = db->Put(leveldb::WriteOptions(), walletToSlice(wallet), amountToSlice(amount));
    if (!status.ok()) throw std::runtime_error("Write failed: " + status.ToString());
}

TransactionAmount Ledger::getWalletValue(const PublicWalletAddress& wallet) const {
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), walletToSlice(wallet), &value);
    
    if (status.ok()) {
        //Logger::logError("Fetched value for wallet: ", walletAddressToString(wallet) + ", Data size: " + std::to_string(value.size()));
    } else {
        //Logger::logError("Failed to fetch value for wallet: ", walletAddressToString(wallet));
    }
    
    if (value.size() != expectedAmountSize) {
        //Logger::logError("Data integrity error: Mismatched transaction amount data size during read. Expected: ", 
        //            std::to_string(expectedAmountSize) + ", Got: " + std::to_string(value.size()));
        //throw std::runtime_error("Data integrity error");
    }

    return *((TransactionAmount*)value.data());  // Using .data() instead of .c_str() for better safety
}

void Ledger::withdraw(const PublicWalletAddress& wallet, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(wallet);

    // Check for underflow
    if (amt > value) {
        Logger::logError("Underflow detected! Attempting to withdraw ", std::to_string(amt) + 
                    " from a wallet with balance " + std::to_string(value));
        throw std::runtime_error("Underflow detected during withdrawal");
    }

    value -= amt;
    this->setWalletValue(wallet, value);
    //Logger::logStatus( YELLOW + "[WITHDRAW]" + "Withdraw of " + std::to_string(amt) + ". New Balance " + std::to_string(value));
}

void Ledger::revertSend(const PublicWalletAddress& wallet, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(wallet);

    // Check for overflow
    if (UINT64_MAX - value < amt) {
        Logger::logError("Overflow detected during revertSend", 
                    "Attempting to add " + std::to_string(amt) + 
                    " to a wallet with balance " + std::to_string(value) + 
                    ". Wallet: ");
        throw std::runtime_error("Overflow detected during revertSend");
    }

    value += amt;
    this->setWalletValue(wallet, value);
    //Logger::logStatus( GREEN + "[REVSEND]" + "RevertSend of " + std::to_string(amt) + ". New Balance " + std::to_string(value));
}

void Ledger::deposit(const PublicWalletAddress& wallet, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(wallet);

    // Check for overflow
    if (UINT64_MAX - value < amt) {
        Logger::logError("Overflow detected during deposit", 
                    "Attempting to deposit " + std::to_string(amt) + 
                    " to a wallet with balance " + std::to_string(value) + 
                    ". Wallet: ");
        throw std::runtime_error("Overflow detected during deposit");
    }

    value += amt;
    this->setWalletValue(wallet, value);
    Logger::logStatus( GREEN + "[DEPOSIT]" + "Deposit of " + std::to_string(amt) + ". New Balance" + std::to_string(value));
}

void Ledger::revertDeposit(PublicWalletAddress to, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(to);

    // Check for underflow
    if (amt > value) {
        Logger::logError("Underflow detected during revertDeposit", 
                    "Attempting to revert deposit of " + std::to_string(amt) + 
                    " from a wallet with balance " + std::to_string(value));
        throw std::runtime_error("Underflow detected during revertDeposit");
    }

    value -= amt;
    this->setWalletValue(to, value);
    //Logger::logStatus( YELLOW + "[REVDEPOSIT]" + "revertDeposit of " + std::to_string(amt) + ". New Balance " + std::to_string(value));
}
