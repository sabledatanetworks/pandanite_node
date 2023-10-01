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

string executionStatusAsString(ExecutionStatus status) {
    switch(status) {
        case SENDER_DOES_NOT_EXIST:
            return "SENDER_DOES_NOT_EXIST";
        break;
        case BALANCE_TOO_LOW:
            return "BALANCE_TOO_LOW";
        break;
        case INVALID_SIGNATURE:
            return "INVALID_SIGNATURE";
        break;
        case INVALID_NONCE:
            return "INVALID_NONCE";
        break;
        case EXTRA_MINING_FEE:
            return "EXTRA_MINING_FEE";
        break;
        case INCORRECT_MINING_FEE:
            return "INCORRECT_MINING_FEE";
        break;
        case INVALID_BLOCK_ID:
            return "INVALID_BLOCK_ID";
        break;
        case NO_MINING_FEE:
            return "NO_MINING_FEE";
        break;
        case UNKNOWN_ERROR:
            return "UNKNOWN_ERROR";
        break;
        case INVALID_TRANSACTION_NONCE:
            return "INVALID_TRANSACTION_NONCE";
        break;
        case INVALID_DIFFICULTY:
            return "INVALID_DIFFICULTY";
        break;
        case INVALID_TRANSACTION_TIMESTAMP:
            return "INVALID_TRANSACTION_TIMESTAMP";
        break;
        case SUCCESS:
            return "SUCCESS";
        break;
        case QUEUE_FULL:
            return "QUEUE_FULL";
        break;
        case EXPIRED_TRANSACTION:
            return "EXPIRED_TRANSACTION";
        break;
        case ALREADY_IN_QUEUE:
            return "ALREADY_IN_QUEUE";
        break;
        case BLOCK_ID_TOO_LARGE:
            return "BLOCK_ID_TOO_LARGE";
        break;
        case INVALID_MERKLE_ROOT:
            return "INVALID_MERKLE_ROOT";
        break;
        case INVALID_LASTBLOCK_HASH:
            return "INVALID_LASTBLOCK_HASH";
        break;
        case BLOCK_TIMESTAMP_TOO_OLD:
            return "BLOCK_TIMESTAMP_TOO_OLD";
        break;
        case HEADER_HASH_INVALID:
            return "HEADER_HASH_INVALID";
        break;
        case INVALID_TRANSACTION_COUNT:
            return "INVALID_TRANSACTION_COUNT";
        case BLOCK_TIMESTAMP_IN_FUTURE:
            return "BLOCK_TIMESTAMP_IN_FUTURE";
        break;
        case WALLET_SIGNATURE_MISMATCH:
            return "WALLET_SIGNATURE_MISMATCH";
        break;
        case TRANSACTION_FEE_TOO_LOW:
            return "TRANSACTION_FEE_TOO_LOW";
        break;
        case IS_SYNCING:
            return "IS_SYNCING";
        break;
    }
}

void deposit(PublicWalletAddress to, TransactionAmount amt, Ledger& ledger,  LedgerState& deltas) {
    if (!ledger.hasWallet(to)) {
        ledger.createWallet(to);
    }
    ledger.deposit(to, amt);
    Logger::logStatus("Deposited VOID: " + std::to_string(amt));
    auto receiverDelta = deltas.find(to);
    if (receiverDelta == deltas.end()) {
        deltas.insert(pair<PublicWalletAddress, TransactionAmount>(to, amt));
        //Logger::logStatus("Delta added for receiver: " + std::to_string(amt));
    } else {
        receiverDelta->second += amt;
        //Logger::logStatus("Delta updated for receiver: " + std::to_string(receiverDelta->second));
    }
}

bool withdraw(PublicWalletAddress from, TransactionAmount amt, Ledger& ledger,  LedgerState & deltas) {
    if (!ledger.hasWallet(from)) {
        return false; // Wallet doesn't exist
    }

    TransactionAmount curr_balance = ledger.getWalletValue(from);
    if (curr_balance < amt) {
	//Logger::logStatus("Status current Balance: " + std::to_string(curr_balance) + " amount parsed: " + std::to_string(amt));
        return false; // Insufficient funds
    }

    // First, try updating the deltas
    auto senderDelta = deltas.find(from);
    if (senderDelta == deltas.end()) {
        deltas.insert(pair<PublicWalletAddress, TransactionAmount>(from, -amt));
        //Logger::logStatus("Delta added for sender: " + std::to_string(-amt));
    } else {
        senderDelta->second -= amt;
	//Logger::logStatus("Delta updated for sender: " + std::to_string(senderDelta->second));
    }

    // After updating the deltas
    ledger.withdraw(from, amt);  // Assuming this doesn't throw exceptions on failure
    //Logger::logStatus("Withdraw VOID: " + std::to_string(amt));
    return true;  // Successful withdrawal
}



ExecutionStatus updateLedger(Transaction t, PublicWalletAddress& miner, Ledger& ledger, LedgerState & deltas, TransactionAmount blockMiningFee, uint32_t blockId) {

    TransactionAmount amt = t.getAmount();
    TransactionAmount fees = t.getTransactionFee();
    PublicWalletAddress to = t.toWallet();
    PublicWalletAddress from = t.fromWallet();

    Logger::logStatus("Processing transaction in block " + std::to_string(blockId) + ". From: " + walletAddressToString(from) + ". To: " + walletAddressToString(to) + ". Amount: " + std::to_string(amt) + ". Fees: " + std::to_string(fees));

    // Log current balances of the sender and receiver wallets
    Logger::logStatus("Initial balance of sender (" + walletAddressToString(from) + "): " + std::to_string(ledger.getWalletValue(from)));
    Logger::logStatus("Initial balance of receiver (" + walletAddressToString(to) + "): " + std::to_string(ledger.getWalletValue(to)));

    if (!t.isFee() && blockId > 1 && walletAddressFromPublicKey(t.getSigningKey()) != t.fromWallet()) {
        Logger::logStatus("Wallet signature mismatch detected for block " + std::to_string(blockId));
        return WALLET_SIGNATURE_MISMATCH;
    }

    if (t.isFee()) {
        Logger::logStatus("Processing fee transaction for block " + std::to_string(blockId));
        if (amt ==  blockMiningFee) {
            deposit(to, amt, ledger, deltas);
            return SUCCESS;
        } else {
            Logger::logStatus("Incorrect mining fee detected for block " + std::to_string(blockId));
            return INCORRECT_MINING_FEE;
        }
    }

    if (blockId == 1) { // special case genesis block
        Logger::logStatus("Special case: genesis block deposit for block " + std::to_string(blockId));
        deposit(to, amt, ledger, deltas);
    } else {
        // from account must exist
        if (!ledger.hasWallet(from)) {
            Logger::logStatus("Sender does not exist for transaction in block " + std::to_string(blockId));
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
	if (!withdraw(from, amt, ledger, deltas)) {
            return BALANCE_TOO_LOW;
        }
        deposit(to, amt, ledger, deltas);

        if (fees > 0) {
	    if (!withdraw(from, fees, ledger, deltas)) {
                return BALANCE_TOO_LOW;
            }
            deposit(miner, fees, ledger, deltas);
        }
    }

    for (auto& delta : deltas) {
    }

    Logger::logStatus("Finished updateLedger for block " + std::to_string(blockId));
    return SUCCESS;
}


void rollbackLedger(Transaction t, PublicWalletAddress& miner, Ledger& ledger) {

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

    Logger::logStatus("Finished Executor::Rollback.");
}


void Executor::RollbackBlock(Block& curr, Ledger& ledger, TransactionStore & txdb) {
    Logger::logStatus("Starting RollbackBlock for block ID: " + std::to_string(curr.getId()));

    PublicWalletAddress miner;
    for(auto t : curr.getTransactions()) {
        if (t.isFee()) {
            miner = t.toWallet();
            Logger::logStatus("Found fee transaction. Miner's wallet: " + walletAddressToString(miner));
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

    Logger::logStatus("Finished RollbackBlock for block ID: " + std::to_string(curr.getId()));
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
