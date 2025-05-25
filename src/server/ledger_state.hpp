#pragma once
#include <map>
#include "../core/common.hpp"

// LedgerState represents a snapshot of wallet balances
using LedgerState = std::map<PublicWalletAddress, TransactionAmount>; 