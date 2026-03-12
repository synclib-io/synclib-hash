/**
 * Test suite for synclib_hash
 *
 * These tests verify that the hashing produces consistent results
 * that match the expected outputs from other platform implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_STR_EQ(expected, actual, msg) do { \
    if (strcmp(expected, actual) == 0) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s\n", msg); \
        printf("    Expected: %s\n", expected); \
        printf("    Actual:   %s\n", actual); \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr, msg) do { \
    if (ptr != NULL) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s (was NULL)\n", msg); \
    } \
} while(0)

/* Test SHA256 */
void test_sha256(void) {
    printf("\n=== Testing SHA256 ===\n");

    /* Empty string */
    char* hash1 = synclib_sha256_hex("", 0);
    ASSERT_NOT_NULL(hash1, "SHA256 of empty string");
    if (hash1) {
        ASSERT_STR_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                      hash1, "Empty string hash value");
        synclib_free(hash1);
    }

    /* "hello" */
    char* hash2 = synclib_sha256_hex("hello", 5);
    ASSERT_NOT_NULL(hash2, "SHA256 of 'hello'");
    if (hash2) {
        ASSERT_STR_EQ("2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
                      hash2, "Hello hash value");
        synclib_free(hash2);
    }
}

/* Test JSON building */
void test_json_building(void) {
    printf("\n=== Testing JSON Building ===\n");

    /* Empty object */
    char* json1 = synclib_build_sorted_json(NULL, 0, NULL, 0);
    ASSERT_NOT_NULL(json1, "Empty JSON object");
    if (json1) {
        ASSERT_STR_EQ("{}", json1, "Empty object value");
        synclib_free(json1);
    }

    /* Simple key-value pairs (should be sorted alphabetically) */
    synclib_kv_t kvs1[] = {
        { .key = "name", .type = SYNCLIB_TYPE_TEXT, .text_value = "Alice" },
        { .key = "age", .type = SYNCLIB_TYPE_INTEGER, .int_value = 30 },
        { .key = "active", .type = SYNCLIB_TYPE_BOOL, .bool_value = 1 },
    };
    char* json2 = synclib_build_sorted_json(kvs1, 3, NULL, 0);
    ASSERT_NOT_NULL(json2, "Simple JSON object");
    if (json2) {
        /* Keys should be sorted: active, age, name */
        ASSERT_STR_EQ("{\"active\":true,\"age\":30,\"name\":\"Alice\"}", json2,
                      "Sorted JSON value");
        synclib_free(json2);
    }

    /* Test with null value */
    synclib_kv_t kvs2[] = {
        { .key = "id", .type = SYNCLIB_TYPE_TEXT, .text_value = "123" },
        { .key = "deleted_at", .type = SYNCLIB_TYPE_NULL },
    };
    char* json3 = synclib_build_sorted_json(kvs2, 2, NULL, 0);
    ASSERT_NOT_NULL(json3, "JSON with null");
    if (json3) {
        ASSERT_STR_EQ("{\"deleted_at\":null,\"id\":\"123\"}", json3,
                      "JSON with null value");
        synclib_free(json3);
    }

    /* Test skip keys */
    synclib_kv_t kvs3[] = {
        { .key = "id", .type = SYNCLIB_TYPE_TEXT, .text_value = "123" },
        { .key = "document", .type = SYNCLIB_TYPE_TEXT, .text_value = "{\"big\":\"json\"}" },
        { .key = "name", .type = SYNCLIB_TYPE_TEXT, .text_value = "Test" },
    };
    const char* skip[] = { "document" };
    char* json4 = synclib_build_sorted_json(kvs3, 3, skip, 1);
    ASSERT_NOT_NULL(json4, "JSON with skip keys");
    if (json4) {
        ASSERT_STR_EQ("{\"id\":\"123\",\"name\":\"Test\"}", json4,
                      "JSON with document skipped");
        synclib_free(json4);
    }

    /* Test special characters in strings */
    synclib_kv_t kvs4[] = {
        { .key = "text", .type = SYNCLIB_TYPE_TEXT, .text_value = "Hello\nWorld\t\"quoted\"" },
    };
    char* json5 = synclib_build_sorted_json(kvs4, 1, NULL, 0);
    ASSERT_NOT_NULL(json5, "JSON with special chars");
    if (json5) {
        ASSERT_STR_EQ("{\"text\":\"Hello\\nWorld\\t\\\"quoted\\\"\"}", json5,
                      "JSON with escaped special chars");
        synclib_free(json5);
    }

    /* Test float formatting (%g removes trailing zeros) */
    synclib_kv_t kvs5[] = {
        { .key = "a", .type = SYNCLIB_TYPE_FLOAT, .float_value = 1.0 },
        { .key = "b", .type = SYNCLIB_TYPE_FLOAT, .float_value = 1.5 },
        { .key = "c", .type = SYNCLIB_TYPE_FLOAT, .float_value = 3.14159 },
    };
    char* json6 = synclib_build_sorted_json(kvs5, 3, NULL, 0);
    ASSERT_NOT_NULL(json6, "JSON with floats");
    if (json6) {
        /* %g format: 1.0 -> "1", 1.5 -> "1.5", 3.14159 -> "3.14159" */
        ASSERT_STR_EQ("{\"a\":1,\"b\":1.5,\"c\":3.14159}", json6,
                      "JSON with float formatting");
        synclib_free(json6);
    }
}

/* Test row hash */
void test_row_hash(void) {
    printf("\n=== Testing Row Hash ===\n");

    /* Row hash from sorted JSON */
    char* hash1 = synclib_row_hash("abc123", "{\"id\":\"abc123\",\"name\":\"Test\"}");
    ASSERT_NOT_NULL(hash1, "Row hash from sorted JSON");
    if (hash1) {
        /* Verify it's a valid 64-char hex string */
        if (strlen(hash1) == 64) {
            tests_passed++;
            printf("  ✓ Hash length is 64 chars\n");
        } else {
            tests_failed++;
            printf("  ✗ Hash length should be 64, got %zu\n", strlen(hash1));
        }
        synclib_free(hash1);
    }

    /* Row hash from key-value pairs */
    synclib_kv_t kvs[] = {
        { .key = "id", .type = SYNCLIB_TYPE_TEXT, .text_value = "abc123" },
        { .key = "name", .type = SYNCLIB_TYPE_TEXT, .text_value = "Test" },
    };
    char* hash2 = synclib_row_hash_from_kvs("abc123", kvs, 2, NULL, 0);
    ASSERT_NOT_NULL(hash2, "Row hash from KVs");
    if (hash2) {
        /* Should match the sorted JSON version */
        char* expected = synclib_row_hash("abc123", "{\"id\":\"abc123\",\"name\":\"Test\"}");
        if (expected) {
            ASSERT_STR_EQ(expected, hash2, "Row hash from KVs matches sorted JSON");
            synclib_free(expected);
        }
        synclib_free(hash2);
    }
}

/* Test block hash */
void test_block_hash(void) {
    printf("\n=== Testing Block Hash ===\n");

    /* Empty block */
    char* hash1 = synclib_block_hash(NULL, 0);
    ASSERT_NOT_NULL(hash1, "Empty block hash");
    if (hash1) {
        /* Empty block should be SHA256("") */
        ASSERT_STR_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                      hash1, "Empty block is SHA256('')");
        synclib_free(hash1);
    }

    /* Block with row hashes */
    const char* row_hashes[] = {
        "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    };
    char* hash2 = synclib_block_hash(row_hashes, 2);
    ASSERT_NOT_NULL(hash2, "Block hash with 2 rows");
    if (hash2) {
        if (strlen(hash2) == 64) {
            tests_passed++;
            printf("  ✓ Block hash length is 64 chars\n");
        } else {
            tests_failed++;
            printf("  ✗ Block hash length should be 64, got %zu\n", strlen(hash2));
        }
        synclib_free(hash2);
    }
}

/* Test merkle root */
void test_merkle_root(void) {
    printf("\n=== Testing Merkle Root ===\n");

    /* Empty tree - returns SHA256("") for consistency with server */
    char* root1 = synclib_merkle_root(NULL, 0);
    ASSERT_NOT_NULL(root1, "Empty merkle root");
    if (root1) {
        ASSERT_STR_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", root1, "Empty tree returns SHA256 of empty string");
        synclib_free(root1);
    }

    /* Single block */
    const char* blocks1[] = {
        "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
    };
    char* root2 = synclib_merkle_root(blocks1, 1);
    ASSERT_NOT_NULL(root2, "Single block merkle root");
    if (root2) {
        ASSERT_STR_EQ("2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
                      root2, "Single block returns block hash");
        synclib_free(root2);
    }

    /* Two blocks */
    const char* blocks2[] = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    };
    char* root3 = synclib_merkle_root(blocks2, 2);
    ASSERT_NOT_NULL(root3, "Two block merkle root");
    if (root3) {
        /* Should be SHA256(block1 + block2) */
        char* expected = synclib_sha256_hex(
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            128);
        if (expected) {
            ASSERT_STR_EQ(expected, root3, "Two block root is SHA256(a+b)");
            synclib_free(expected);
        }
        synclib_free(root3);
    }

    /* Three blocks (odd count - third is passed up) */
    const char* blocks3[] = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
    };
    char* root4 = synclib_merkle_root(blocks3, 3);
    ASSERT_NOT_NULL(root4, "Three block merkle root");
    if (root4) {
        if (strlen(root4) == 64) {
            tests_passed++;
            printf("  ✓ Three block root has correct length\n");
        } else {
            tests_failed++;
            printf("  ✗ Three block root should be 64 chars, got %zu\n", strlen(root4));
        }
        synclib_free(root4);
    }
}

int main(void) {
    printf("=== synclib_hash Test Suite ===\n");

    test_sha256();
    test_json_building();
    test_row_hash();
    test_block_hash();
    test_merkle_root();

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
