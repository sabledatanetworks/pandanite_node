#pragma once
#include "constants.hpp"
#include "common.hpp"
#include "crypto.hpp"
#include <optional>
using namespace std;

struct TransactionInfo {
    char signature[64];
    char signingKey[32];
    uint64_t timestamp;
    PublicWalletAddress to;
    PublicWalletAddress from;
    TransactionAmount amount;
    TransactionAmount fee;
    bool isTransactionFee;
};

#define TRANSACTIONINFO_BUFFER_SIZE 149

TransactionInfo transactionInfoFromBuffer(const char* buffer);
void transactionInfoToBuffer(TransactionInfo& t, char* buffer);

class Transaction {
    public:
        static const uint64_t TRANSACTION_EXPIRY = 3600; // 1 hour expiry
        
        Transaction(json t);
        Transaction();
        Transaction(const Transaction & t);
        Transaction(PublicWalletAddress to, TransactionAmount fee);
        Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee=0);
        Transaction(PublicWalletAddress from, PublicWalletAddress to, TransactionAmount amount, PublicKey signingKey, TransactionAmount fee, uint64_t timestamp);
        Transaction(const TransactionInfo& t);
        TransactionInfo serialize() const;
        json toJson();
        void sign(PublicKey pubKey, PrivateKey signingKey);
        void setTransactionFee(TransactionAmount amount);
        TransactionAmount getTransactionFee() const;
        void setAmount(TransactionAmount amt);
        PublicKey getSigningKey();
        PublicWalletAddress fromWallet() const;
        PublicWalletAddress toWallet() const;
        TransactionAmount getAmount() const;
        TransactionAmount getFee() const;
        void setTimestamp(uint64_t t);
        uint64_t getTimestamp() const;
        SHA256Hash getHash() const;
        SHA256Hash hashContents() const;
        TransactionSignature getSignature() const;
        bool signatureValid() const;
        bool isFee() const;
        bool isExpired() const;
        uint64_t getNonce() const;
        void setNonce(uint64_t nonce);
    protected:
        TransactionSignature signature;
        PublicKey signingKey;
        uint64_t timestamp;
        PublicWalletAddress to;
        PublicWalletAddress from;
        TransactionAmount amount;
        TransactionAmount fee;
        bool isTransactionFee;
        uint64_t nonce;
        friend bool operator==(const Transaction& a, const Transaction& b);
        friend bool operator<(const Transaction& a, const Transaction& b);
};

bool operator==(const Transaction& a, const Transaction& b);

