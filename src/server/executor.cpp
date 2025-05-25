#include <map>
#include <optional>
#include <iostream>
#include <fstream>
#include <set>
#include <cmath>
#include <mutex>
#include "../core/block.hpp"
#include "../core/logger.hpp"
#include "../core/helpers.hpp"
#include "executor.hpp"

using namespace std;

json invalidTransactions = readJsonFromFile("invalid.json");

std::mutex invalidTxMutex;

void addInvalidTransaction(uint64_t blockId, PublicWalletAddress wallet) {
    std::lock_guard<std::mutex> lock(invalidTxMutex);

    json newInvalidEntry = {
        {"block", blockId},
        {"wallet", walletAddressToString(wallet)}
    };

    // Idempotency check
    for (auto& entry : invalidTransactions) {
        if (entry == newInvalidEntry) {
            return;
        }
    }

    invalidTransactions.push_back(newInvalidEntry);

    // Write updated data back to the file
    ofstream outFile("./invalid.json");
    if (outFile.is_open()) {
        outFile << invalidTransactions.dump(4);
        if (outFile.fail()) {
            Logger::logError(RED + "[ERROR]" + RESET, "Failed to write to invalid.json.");
            return;
        }
        outFile.close();
    } else {
        Logger::logError(RED + "[ERROR]" + RESET, "Failed to open invalid.json for writing.");
    }

    std::string backupFileName = "./invalid_backup.json";
    ofstream backupFile(backupFileName);
    if (backupFile.is_open()) {
        backupFile << invalidTransactions.dump(4);
        backupFile.close();
        Logger::logStatus(GREEN + "[INFO]" + RESET + " Backup updated: " + backupFileName);
    } else {
        Logger::logError(RED + "[ERROR]" + RESET, "Failed to update backup: " + backupFileName);
    }
}

bool isInvalidTransaction(uint64_t blockId, PublicWalletAddress wallet) {
    for(auto item : invalidTransactions) {
        if (item["wallet"] == walletAddressToString(wallet) && item["block"] == blockId) return true;
    }
    return false;
}

std::string executionStatusAsString(ExecutionStatus status) {
    switch(status) {
        case SENDER_DOES_NOT_EXIST:
            return "SENDER_DOES_NOT_EXIST";
        case BALANCE_TOO_LOW:
            return "BALANCE_TOO_LOW";
        case INVALID_SIGNATURE:
            return "INVALID_SIGNATURE";
        case INVALID_NONCE:
            return "INVALID_NONCE";
        case EXTRA_MINING_FEE:
            return "EXTRA_MINING_FEE";
        case INCORRECT_MINING_FEE:
            return "INCORRECT_MINING_FEE";
        case INVALID_BLOCK_ID:
            return "INVALID_BLOCK_ID";
        case NO_MINING_FEE:
            return "NO_MINING_FEE";
        case INVALID_DIFFICULTY:
            return "INVALID_DIFFICULTY";
        case INVALID_TRANSACTION_NONCE:
            return "INVALID_TRANSACTION_NONCE";
        case INVALID_TRANSACTION_TIMESTAMP:
            return "INVALID_TRANSACTION_TIMESTAMP";
        case BLOCK_TIMESTAMP_TOO_OLD:
            return "BLOCK_TIMESTAMP_TOO_OLD";
        case BLOCK_TIMESTAMP_IN_FUTURE:
            return "BLOCK_TIMESTAMP_IN_FUTURE";
        case UNKNOWN_ERROR:
            return "UNKNOWN_ERROR";
        case QUEUE_FULL:
            return "QUEUE_FULL";
        case HEADER_HASH_INVALID:
            return "HEADER_HASH_INVALID";
        case EXPIRED_TRANSACTION:
            return "EXPIRED_TRANSACTION";
        case ALREADY_IN_QUEUE:
            return "ALREADY_IN_QUEUE";
        case BLOCK_ID_TOO_LARGE:
            return "BLOCK_ID_TOO_LARGE";
        case INVALID_MERKLE_ROOT:
            return "INVALID_MERKLE_ROOT";
        case INVALID_LASTBLOCK_HASH:
            return "INVALID_LASTBLOCK_HASH";
        case INVALID_TRANSACTION_COUNT:
            return "INVALID_TRANSACTION_COUNT";
        case TRANSACTION_FEE_TOO_LOW:
            return "TRANSACTION_FEE_TOO_LOW";
        case WALLET_SIGNATURE_MISMATCH:
            return "WALLET_SIGNATURE_MISMATCH";
        case IS_SYNCING:
            return "IS_SYNCING";
        case SUCCESS:
            return "SUCCESS";
        case INSUFFICIENT_FUNDS:
            return "INSUFFICIENT_FUNDS";
        default:
            return "UNKNOWN_ERROR";
    }
}

void deposit(PublicWalletAddress to, TransactionAmount amt, Ledger& ledger,  LedgerState& deltas) {
    if (!ledger.hasWallet(to)) {
        ledger.createWallet(to);   
    }
    ledger.deposit(to, amt);
    auto receiverDelta = deltas.find(to);
    if (receiverDelta == deltas.end()) {
        deltas.insert(pair<PublicWalletAddress, TransactionAmount>(to, amt));
    } else {
        receiverDelta->second += amt;
    }
}

void withdraw(PublicWalletAddress from, TransactionAmount amt, Ledger& ledger,  LedgerState & deltas) {
    if (ledger.hasWallet(from)) {
        ledger.withdraw(from, amt);
    } else {
        throw std::runtime_error("Tried withdrawing from non-existant account");
    }

    auto senderDelta = deltas.find(from);
    if (senderDelta == deltas.end()) {
        deltas.insert(pair<PublicWalletAddress, TransactionAmount>(from, -amt));
    } else {
        senderDelta->second -= amt;
    }
}

ExecutionStatus updateLedger(Transaction t, PublicWalletAddress& miner, Ledger& ledger, LedgerState & deltas, TransactionAmount blockMiningFee, uint32_t blockId) {
    TransactionAmount amt = t.getAmount();
    TransactionAmount fees = t.getTransactionFee();
    PublicWalletAddress to = t.toWallet();
    PublicWalletAddress from = t.fromWallet();

    if (!t.isFee() && blockId > 1 && walletAddressFromPublicKey(t.getSigningKey()) != t.fromWallet()) {
        return WALLET_SIGNATURE_MISMATCH;
    }
    
    if (t.isFee()) {
        if (amt ==  blockMiningFee) {
            deposit(to, amt, ledger, deltas);
            return SUCCESS;
        } else {
            return INCORRECT_MINING_FEE;
        }
    }

    if (blockId == 1) { // special case genesis block
        deposit(to, amt, ledger, deltas);
    } else {
        // from account must exist
        if (!ledger.hasWallet(from)) {
            return SENDER_DOES_NOT_EXIST;
        }
        TransactionAmount total = ledger.getWalletValue(from);

        if (total < amt) {
            if (!isInvalidTransaction(blockId, from) && blockId != 0) {
                addInvalidTransaction(blockId, from);
                return BALANCE_TOO_LOW;
            } 
        }

        total -= amt;

        if (total < fees) {
            if (!isInvalidTransaction(blockId, from) && blockId != 0) {
                addInvalidTransaction(blockId, from);
                return BALANCE_TOO_LOW;
            } 
        }
        withdraw(from, amt, ledger, deltas);
        deposit(to, amt, ledger, deltas);
        if (fees > 0) {
            withdraw(from, fees, ledger, deltas);
            deposit(miner, fees, ledger, deltas);
        }
    }

    return SUCCESS;
}

void rollbackLedger(Transaction t,  PublicWalletAddress& miner, Ledger& ledger) {
    TransactionAmount amt = t.getAmount();
    TransactionAmount fees = t.getTransactionFee();
    PublicWalletAddress to = t.toWallet();
    PublicWalletAddress from = t.fromWallet();
    if (t.isFee()) {
        ledger.revertDeposit(to, amt);
    } else {
        if (fees > 0) {
            ledger.revertDeposit(miner, fees);
            ledger.revertSend(from, fees);
        }
        ledger.revertDeposit(to, amt);
        ledger.revertSend(from, amt);
    }
}

void Executor::Rollback(Ledger& ledger, LedgerState& deltas) {
    for(auto it : deltas) {
        ledger.withdraw(it.first, it.second);
    }
}

void Executor::RollbackBlock(Block& curr, Ledger& ledger, TransactionStore & txdb) {
    PublicWalletAddress miner;
    for(auto t : curr.getTransactions()) {
        if (t.isFee()) {
            miner = t.toWallet();
            break;
        }
    }
    for(int i = curr.getTransactions().size() - 1; i >=0; i--) {
        Transaction t = curr.getTransactions()[i];
        rollbackLedger(t, miner, ledger);
        if (!t.isFee()) {
            txdb.removeTransaction(t);
        }
    }
}

ExecutionStatus Executor::ExecuteTransaction(Ledger& ledger, Transaction t,  LedgerState& deltas) {
    if (!t.isFee() && !t.signatureValid()) {
        return INVALID_SIGNATURE;
    }

    if (!t.isFee() && walletAddressFromPublicKey(t.getSigningKey()) != t.fromWallet()) {
        return WALLET_SIGNATURE_MISMATCH;
    }

    PublicWalletAddress miner = NULL_ADDRESS;
    return updateLedger(t, miner, ledger, deltas, PDN(0), 0); // ExecuteTransaction is only used on non-fee transactions
}

ExecutionStatus Executor::ExecuteBlock(Block& curr, Ledger& ledger, TransactionStore & txdb, LedgerState& deltas, TransactionAmount blockMiningFee) {
    // try executing each transaction
    bool foundFee = false;
    PublicWalletAddress miner;
    TransactionAmount miningFee;
    std::set<SHA256Hash> transactionHashes; // avoid duplicate transactions within block
    for(auto t : curr.getTransactions()) {
        if (t.isFee()) {
            if (foundFee) return EXTRA_MINING_FEE;
            miner = t.toWallet();
            miningFee = t.getAmount();
            foundFee = true;
        } else if (
                (transactionHashes.insert(t.getHash()).second == false ) 
                || (txdb.hasTransaction(t) && curr.getId() != 1)) 
        {
            return EXPIRED_TRANSACTION;
        }
        
        if (!t.isFee() && curr.getId() > 1 && walletAddressFromPublicKey(t.getSigningKey()) != t.fromWallet()) {
            return WALLET_SIGNATURE_MISMATCH;
        }
    }
    if (!foundFee) {
        return NO_MINING_FEE;
    }

    if (miningFee != blockMiningFee) {
        return INCORRECT_MINING_FEE;
    }
    for(auto t : curr.getTransactions()) {
        if (!t.isFee() && !t.signatureValid() && curr.getId() != 1) {
            return INVALID_SIGNATURE;
        }
        ExecutionStatus updateStatus = updateLedger(t, miner, ledger, deltas, blockMiningFee, curr.getId());
        if (updateStatus != SUCCESS) {
            return updateStatus;
        }
    }
    return SUCCESS;
}

