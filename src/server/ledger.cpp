#include <iostream>
#include <thread>
#include "../core/crypto.hpp"
#include "ledger.hpp"
using namespace std;


Ledger::Ledger() {
}

leveldb::Slice walletToSlice(const PublicWalletAddress& w) {
    leveldb::Slice s2 = leveldb::Slice((const char*) w.data(), w.size());
    return s2;
}

leveldb::Slice amountToSlice(const TransactionAmount& t) {
    leveldb::Slice s2 = leveldb::Slice((const char*) &t, sizeof(TransactionAmount));
    return s2;
}

bool Ledger::hasWallet(const PublicWalletAddress& wallet) const{
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), walletToSlice(wallet), &value);
    return (status.ok());
}

void Ledger::createWallet(const PublicWalletAddress& wallet) {
    if(this->hasWallet(wallet)) throw std::runtime_error("Wallet exists");
    this->setWalletValue(wallet, 0);
}

void Ledger::setWalletValue(const PublicWalletAddress& wallet, TransactionAmount amount) {
    leveldb::Status status = db->Put(leveldb::WriteOptions(), walletToSlice(wallet), amountToSlice(amount));
    if (!status.ok()) throw std::runtime_error("Write failed: " + status.ToString());
}

TransactionAmount Ledger::getWalletValue(const PublicWalletAddress& wallet) const{
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), walletToSlice(wallet), &value);
    if(!status.ok()) throw std::runtime_error("Tried fetching wallet value for non-existant wallet");
    return *((TransactionAmount*)value.c_str());
}

void Ledger::withdraw(const PublicWalletAddress& wallet, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(wallet);
    if (amt > value) {
        throw std::runtime_error("Insufficient balance");
    }
    if (value - amt > value) { // Check for underflow
        throw std::runtime_error("Balance underflow");
    }
    value -= amt;
    this->setWalletValue(wallet, value);
}

void Ledger::revertSend(const PublicWalletAddress& wallet, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(wallet);
    this->setWalletValue(wallet, value + amt);
}

void Ledger::deposit(const PublicWalletAddress& wallet, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(wallet);
    if (value + amt < value) { // Check for overflow
        throw std::runtime_error("Balance overflow");
    }
    this->setWalletValue(wallet, value + amt);
}

void Ledger::revertDeposit(PublicWalletAddress to, TransactionAmount amt) {
    TransactionAmount value = this->getWalletValue(to);
    this->setWalletValue(to, value - amt);
}

bool Ledger::atomicWithdrawIfSufficient(const PublicWalletAddress& wallet, TransactionAmount amt) {
    std::lock_guard<std::mutex> lock(ledger_mutex);
    try {
        TransactionAmount value = this->getWalletValue(wallet);
        if (amt > value || value - amt > value) {
            return false;
        }
        this->setWalletValue(wallet, value - amt);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Ledger::atomicDepositIfValid(const PublicWalletAddress& wallet, TransactionAmount amt) {
    std::lock_guard<std::mutex> lock(ledger_mutex);
    try {
        TransactionAmount value = this->getWalletValue(wallet);
        if (value + amt < value) {
            return false;
        }
        this->setWalletValue(wallet, value + amt);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void Ledger::clear() {
    std::lock_guard<std::mutex> lock(ledger_mutex);
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        db->Delete(leveldb::WriteOptions(), it->key());
    }
    delete it;
}

LedgerState Ledger::getState() const {
    std::lock_guard<std::mutex> lock(ledger_mutex);
    LedgerState state;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        PublicWalletAddress wallet;
        std::memcpy(wallet.data(), it->key().data(), wallet.size());
        TransactionAmount amount = *((TransactionAmount*)it->value().data());
        state[wallet] = amount;
    }
    delete it;
    return state;
}

uint64_t Ledger::getWalletNonce(const PublicWalletAddress& wallet) const {
    std::lock_guard<std::mutex> lockGuard(lock);
    auto it = nonces.find(wallet);
    if (it == nonces.end()) {
        return 0;
    }
    return it->second;
}

void Ledger::incrementWalletNonce(const PublicWalletAddress& wallet) {
    std::lock_guard<std::mutex> lockGuard(lock);
    if (!hasWallet(wallet)) {
        createWallet(wallet);
    }
    nonces[wallet]++;
}

Ledger::~Ledger() {
    closeDB();
}
