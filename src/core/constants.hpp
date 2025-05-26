#pragma once
#include <string>
#include <cstdint>

// System
#define DECIMAL_SCALE_FACTOR 10000
#define TIMEOUT_MS 5000
#define TIMEOUT_BLOCK_MS 30000
#define TIMEOUT_BLOCKHEADERS_MS 60000
#define TIMEOUT_SUBMIT_MS 30000
#define BLOCKS_PER_FETCH 200
#define BLOCK_HEADERS_PER_FETCH 2000

// Include generated version
#include "version.h"

// Files
#define LEDGER_FILE_PATH "./data/ledger"
#define TXDB_FILE_PATH "./data/txdb"
#define BLOCK_STORE_FILE_PATH "./data/blocks"
#define PUFFERFISH_CACHE_FILE_PATH "./data/pufferfish"

// Blocks
#define MAX_TRANSACTIONS_PER_BLOCK 25000

// Difficulty
#define DIFFICULTY_LOOKBACK 100
#define DESIRED_BLOCK_TIME_SEC 90
#define MIN_DIFFICULTY 6
#define MAX_DIFFICULTY 255

// Network constants
#define DEFAULT_PORT 8000
#define DEFAULT_RPC_PORT 8001
#define DEFAULT_WS_PORT 8002
#define DEFAULT_HOST "localhost"
#define DEFAULT_RPC_HOST "localhost"
#define DEFAULT_WS_HOST "localhost"

// Blockchain constants
#define GENESIS_BLOCK_ID 0
#define PUFFERFISH_START_BLOCK 1  // Pufferfish mining algorithm starts from block 1

// File paths
#define BLOCKCHAIN_DB_PATH "./data/blockchain"
#define WALLET_DB_PATH "./data/wallet"

