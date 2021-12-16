#pragma once

// System
#define DECIMAL_SCALE_FACTOR 10000
#define TIMEOUT_MS 5000
#define TIMEOUT_SUBMIT_MS 45000
#define BLOCKS_PER_FETCH 200
#define DEFAULT_BLOOMFILTER_BITS 8000
#define DEFAULT_BLOOMFILTER_HASHES 5

// Files
#define GENESIS_FILE_PATH "./genesis.json"
#define DEFAULT_GENESIS_USER_PATH "./keys/miner.json"
#define DEFAULT_CONFIG_FILE_PATH "./config.json"
#define LEDGER_FILE_PATH "./data/ledger"
#define TXDB_FILE_PATH "./data/txdb"
#define BLOCK_STORE_FILE_PATH "./data/blocks"

// Blocks
#define MAX_TRANSACTIONS_PER_BLOCK 10000
#define DIFFICULTY_RESET_FREQUENCY 100
#define TRANSACTION_NONCE_SIZE 8
#define TIMESTAMP_VERIFICATION_START 20700
#define NEW_DIFFICULTY_COMPUTATION_BLOCK 28000

// Payments`
#define MINING_FEE (50 * DECIMAL_SCALE_FACTOR)
#define MINING_PAYMENTS_UNTIL 2000000

// Difficulty
#define DESIRED_BLOCK_TIME_SEC 30
#define MIN_DIFFICULTY 16
#define MAX_DIFFICULTY 256
#define FOUNDER_WALLET_BITS 20

