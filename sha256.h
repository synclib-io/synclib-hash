/**
 * SHA-256 implementation for synclib Merkle tree hashing
 *
 * This is a standalone, portable SHA-256 implementation suitable for
 * embedded systems and cross-platform builds.
 *
 * Based on public domain implementations.
 */

#ifndef SYNCLIB_SHA256_H
#define SYNCLIB_SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_BLOCK_SIZE 32  /* SHA256 outputs 32 bytes (256 bits) */

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

/**
 * Initialize SHA256 context
 */
void sha256_init(SHA256_CTX *ctx);

/**
 * Update context with data
 */
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);

/**
 * Finalize and get hash
 */
void sha256_final(SHA256_CTX *ctx, uint8_t *hash);

/**
 * Convenience function: hash data in one call
 *
 * @param data Input data
 * @param len Length of input data
 * @param hash Output buffer (must be at least 32 bytes)
 */
void sha256_hash(const uint8_t *data, size_t len, uint8_t *hash);

/**
 * Convert hash to hex string
 *
 * @param hash Input hash (32 bytes)
 * @param hex_out Output buffer (must be at least 65 bytes for null terminator)
 */
void sha256_to_hex(const uint8_t *hash, char *hex_out);

/**
 * Convenience function: hash data and return hex string
 *
 * @param data Input data
 * @param len Length of input data
 * @param hex_out Output buffer (must be at least 65 bytes)
 */
void sha256_hash_hex(const uint8_t *data, size_t len, char *hex_out);

#ifdef __cplusplus
}
#endif

#endif /* SYNCLIB_SHA256_H */
