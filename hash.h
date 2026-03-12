/**
 * synclib_hash - Cross-platform Merkle tree hashing library
 *
 * This library provides consistent hashing across all platforms:
 * - Native (iOS, Android, macOS, Linux, Windows) via C
 * - Web via WebAssembly
 * - Server (Elixir) via WebAssembly + Wasmex
 *
 * No SQLite dependency - operates on raw key-value data.
 *
 * Hash format:
 * - Row hash: SHA256(row_id + "|" + sorted_json(row_data)) -> lowercase hex
 * - Block hash: SHA256(concat of row hash hex strings) -> lowercase hex
 * - Merkle root: Binary tree of block hashes, odd node passed up as-is
 */

#ifndef SYNCLIB_HASH_H
#define SYNCLIB_HASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Value types for key-value pairs */
typedef enum {
    SYNCLIB_TYPE_NULL = 0,
    SYNCLIB_TYPE_INTEGER = 1,
    SYNCLIB_TYPE_FLOAT = 2,
    SYNCLIB_TYPE_TEXT = 3,
    SYNCLIB_TYPE_BOOL = 4
} synclib_value_type_t;

/* Key-value pair for row data */
typedef struct {
    const char* key;
    synclib_value_type_t type;
    union {
        int64_t int_value;
        double float_value;
        const char* text_value;
        int bool_value;  /* 0 = false, 1 = true */
    };
} synclib_kv_t;

/**
 * Build sorted JSON string from key-value pairs.
 *
 * Keys are sorted alphabetically for consistent cross-platform hashing.
 * Output format matches what all platforms expect.
 *
 * @param kvs Array of key-value pairs
 * @param count Number of key-value pairs
 * @param skip_keys Array of keys to skip (e.g., ["document"]), or NULL
 * @param skip_count Number of keys to skip
 * @return Allocated JSON string (caller must free), or NULL on error
 */
char* synclib_build_sorted_json(
    const synclib_kv_t* kvs,
    int count,
    const char** skip_keys,
    int skip_count
);

/**
 * Compute row hash.
 *
 * Format: SHA256(row_id + "|" + sorted_json) -> lowercase hex
 *
 * @param row_id The row's primary key value
 * @param sorted_json Pre-built sorted JSON string
 * @return Allocated 65-byte hex string (caller must free), or NULL on error
 */
char* synclib_row_hash(const char* row_id, const char* sorted_json);

/**
 * Compute row hash from key-value pairs.
 *
 * Convenience function that builds sorted JSON internally.
 *
 * @param row_id The row's primary key value
 * @param kvs Array of key-value pairs
 * @param count Number of key-value pairs
 * @param skip_keys Array of keys to skip, or NULL
 * @param skip_count Number of keys to skip
 * @return Allocated 65-byte hex string (caller must free), or NULL on error
 */
char* synclib_row_hash_from_kvs(
    const char* row_id,
    const synclib_kv_t* kvs,
    int count,
    const char** skip_keys,
    int skip_count
);

/**
 * Compute block hash from row hashes.
 *
 * Format: SHA256(row_hash_1 + row_hash_2 + ... + row_hash_n) -> lowercase hex
 *
 * @param row_hashes Array of 64-character hex hash strings
 * @param count Number of row hashes
 * @return Allocated 65-byte hex string (caller must free), or NULL on error
 */
char* synclib_block_hash(const char** row_hashes, int count);

/**
 * Build Merkle root from block hashes.
 *
 * Uses binary tree structure. Odd nodes are passed up as-is (not duplicated).
 *
 * @param block_hashes Array of 64-character hex hash strings
 * @param count Number of block hashes
 * @return Allocated 65-byte hex string (caller must free), or NULL on error
 */
char* synclib_merkle_root(const char** block_hashes, int count);

/**
 * Compute SHA256 hash and return as lowercase hex string.
 *
 * @param data Input data
 * @param len Length of input data
 * @return Allocated 65-byte hex string (caller must free), or NULL on error
 */
char* synclib_sha256_hex(const char* data, size_t len);

/**
 * Build canonical sorted JSON from input JSON string.
 *
 * This is the SINGLE SOURCE OF TRUTH for sorted JSON building across all
 * platforms (Elixir, TypeScript, Dart). All platforms should call this
 * function instead of implementing their own JSON sorting.
 *
 * - Keys are sorted alphabetically (case-sensitive, ASCII order)
 * - Nested objects are sorted recursively
 * - Skip keys are excluded from output
 * - Output format: {"key1":value1,"key2":value2,...}
 *
 * @param input_json JSON string to parse and sort
 * @param skip_keys Array of keys to exclude from output, or NULL
 * @param skip_count Number of keys to skip
 * @return Allocated sorted JSON string (caller must free via synclib_free), or NULL on error
 *
 * Example:
 *   Input:  {"name":"Alice","age":30,"document":{...}}
 *   Skip:   ["document"]
 *   Output: {"age":30,"name":"Alice"}
 */
char* synclib_build_sorted_json_from_json(
    const char* input_json,
    const char** skip_keys,
    int skip_count
);

/**
 * Compute row hash directly from JSON input.
 *
 * Convenience function that builds sorted JSON and computes hash in one call.
 * Format: SHA256(row_id + "|" + sorted_json(input_json)) -> lowercase hex
 *
 * @param row_id The row's primary key value
 * @param input_json JSON string of row data
 * @param skip_keys Array of keys to skip, or NULL
 * @param skip_count Number of keys to skip
 * @return Allocated 65-byte hex string (caller must free via synclib_free), or NULL on error
 */
char* synclib_row_hash_from_json(
    const char* row_id,
    const char* input_json,
    const char** skip_keys,
    int skip_count
);

/**
 * Free a string allocated by this library.
 */
void synclib_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* SYNCLIB_HASH_H */
