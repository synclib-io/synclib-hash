/**
 * synclib_hash - Cross-platform Merkle tree hashing implementation
 */

#include "hash.h"
#include "sha256.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Initial buffer size for JSON building */
#define INITIAL_JSON_CAPACITY 4096

/* ========================================================================== */
/* String Helpers                                                             */
/* ========================================================================== */

/* Compare function for qsort - sorts key-value pairs by key */
static int compare_kv_by_key(const void* a, const void* b) {
    const synclib_kv_t* kv_a = (const synclib_kv_t*)a;
    const synclib_kv_t* kv_b = (const synclib_kv_t*)b;
    return strcmp(kv_a->key, kv_b->key);
}

/* Check if a key is in the skip list */
static int should_skip_key(const char* key, const char** skip_keys, int skip_count) {
    for (int i = 0; i < skip_count; i++) {
        if (strcmp(key, skip_keys[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Escape a string for JSON output */
static char* json_escape_string(const char* str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    /* Worst case: every char needs escaping (\uXXXX = 6 chars) + quotes + null */
    size_t capacity = len * 6 + 3;
    char* result = (char*)malloc(capacity);
    if (!result) return NULL;

    size_t pos = 0;
    result[pos++] = '"';

    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)str[i];

        switch (ch) {
            case '"':
                result[pos++] = '\\';
                result[pos++] = '"';
                break;
            case '\\':
                result[pos++] = '\\';
                result[pos++] = '\\';
                break;
            case '\b':
                result[pos++] = '\\';
                result[pos++] = 'b';
                break;
            case '\f':
                result[pos++] = '\\';
                result[pos++] = 'f';
                break;
            case '\n':
                result[pos++] = '\\';
                result[pos++] = 'n';
                break;
            case '\r':
                result[pos++] = '\\';
                result[pos++] = 'r';
                break;
            case '\t':
                result[pos++] = '\\';
                result[pos++] = 't';
                break;
            default:
                if (ch < 32) {
                    /* Control character - escape as \uXXXX */
                    pos += snprintf(result + pos, 7, "\\u%04x", ch);
                } else {
                    result[pos++] = ch;
                }
                break;
        }
    }

    result[pos++] = '"';
    result[pos] = '\0';
    return result;
}

/* ========================================================================== */
/* JSON Building                                                              */
/* ========================================================================== */

char* synclib_build_sorted_json(
    const synclib_kv_t* kvs,
    int count,
    const char** skip_keys,
    int skip_count
) {
    if (count == 0) {
        char* empty = (char*)malloc(3);
        if (empty) strcpy(empty, "{}");
        return empty;
    }

    /* Make a sorted copy of the key-value pairs */
    synclib_kv_t* sorted = (synclib_kv_t*)malloc(count * sizeof(synclib_kv_t));
    if (!sorted) return NULL;
    memcpy(sorted, kvs, count * sizeof(synclib_kv_t));
    qsort(sorted, count, sizeof(synclib_kv_t), compare_kv_by_key);

    /* Build JSON string */
    size_t capacity = INITIAL_JSON_CAPACITY;
    char* json = (char*)malloc(capacity);
    if (!json) {
        free(sorted);
        return NULL;
    }

    size_t pos = 0;
    json[pos++] = '{';

    int first = 1;
    for (int i = 0; i < count; i++) {
        const synclib_kv_t* kv = &sorted[i];

        /* Skip specified keys */
        if (skip_keys && should_skip_key(kv->key, skip_keys, skip_count)) {
            continue;
        }

        /* Add comma separator */
        if (!first) {
            json[pos++] = ',';
        }
        first = 0;

        /* Add key */
        char* escaped_key = json_escape_string(kv->key);
        if (!escaped_key) {
            free(json);
            free(sorted);
            return NULL;
        }
        size_t key_len = strlen(escaped_key);

        /* Ensure capacity */
        if (pos + key_len + 100 >= capacity) {
            capacity = (pos + key_len + 100) * 2;
            char* new_json = (char*)realloc(json, capacity);
            if (!new_json) {
                free(escaped_key);
                free(json);
                free(sorted);
                return NULL;
            }
            json = new_json;
        }

        memcpy(json + pos, escaped_key, key_len);
        pos += key_len;
        free(escaped_key);

        json[pos++] = ':';

        /* Add value based on type */
        switch (kv->type) {
            case SYNCLIB_TYPE_NULL:
                memcpy(json + pos, "null", 4);
                pos += 4;
                break;

            case SYNCLIB_TYPE_INTEGER:
                pos += snprintf(json + pos, 32, "%lld", (long long)kv->int_value);
                break;

            case SYNCLIB_TYPE_FLOAT: {
                /* Use %g to avoid trailing zeros, matching other platforms */
                pos += snprintf(json + pos, 32, "%g", kv->float_value);
                break;
            }

            case SYNCLIB_TYPE_BOOL:
                if (kv->bool_value) {
                    memcpy(json + pos, "true", 4);
                    pos += 4;
                } else {
                    memcpy(json + pos, "false", 5);
                    pos += 5;
                }
                break;

            case SYNCLIB_TYPE_TEXT: {
                char* escaped = json_escape_string(kv->text_value);
                if (!escaped) {
                    free(json);
                    free(sorted);
                    return NULL;
                }
                size_t val_len = strlen(escaped);

                /* Ensure capacity */
                if (pos + val_len + 10 >= capacity) {
                    capacity = (pos + val_len + 10) * 2;
                    char* new_json = (char*)realloc(json, capacity);
                    if (!new_json) {
                        free(escaped);
                        free(json);
                        free(sorted);
                        return NULL;
                    }
                    json = new_json;
                }

                memcpy(json + pos, escaped, val_len);
                pos += val_len;
                free(escaped);
                break;
            }
        }

        /* Ensure capacity for next iteration */
        if (pos + 256 >= capacity) {
            capacity *= 2;
            char* new_json = (char*)realloc(json, capacity);
            if (!new_json) {
                free(json);
                free(sorted);
                return NULL;
            }
            json = new_json;
        }
    }

    json[pos++] = '}';
    json[pos] = '\0';

    free(sorted);
    return json;
}

/* ========================================================================== */
/* JSON from JSON (Single Source of Truth)                                    */
/* ========================================================================== */

/* Compare function for sorting string keys */
static int compare_strings(const void* a, const void* b) {
    const char* str_a = *(const char**)a;
    const char* str_b = *(const char**)b;
    return strcmp(str_a, str_b);
}

/* Forward declaration for recursive encoding */
static char* encode_cjson_value(const cJSON* item);

/* Encode a cJSON object with sorted keys */
static char* encode_cjson_object_sorted(const cJSON* obj, const char** skip_keys, int skip_count) {
    if (!cJSON_IsObject(obj)) return NULL;

    /* Count keys (excluding skip_keys) */
    int key_count = 0;
    const cJSON* child = obj->child;
    while (child) {
        if (!should_skip_key(child->string, skip_keys, skip_count)) {
            key_count++;
        }
        child = child->next;
    }

    if (key_count == 0) {
        char* empty = (char*)malloc(3);
        if (empty) strcpy(empty, "{}");
        return empty;
    }

    /* Collect and sort keys */
    const char** keys = (const char**)malloc(key_count * sizeof(char*));
    if (!keys) return NULL;

    int idx = 0;
    child = obj->child;
    while (child) {
        if (!should_skip_key(child->string, skip_keys, skip_count)) {
            keys[idx++] = child->string;
        }
        child = child->next;
    }

    qsort(keys, key_count, sizeof(char*), compare_strings);

    /* Build JSON string */
    size_t capacity = INITIAL_JSON_CAPACITY;
    char* json = (char*)malloc(capacity);
    if (!json) {
        free(keys);
        return NULL;
    }

    size_t pos = 0;
    json[pos++] = '{';

    for (int i = 0; i < key_count; i++) {
        /* Add comma separator */
        if (i > 0) {
            json[pos++] = ',';
        }

        /* Add key */
        char* escaped_key = json_escape_string(keys[i]);
        if (!escaped_key) {
            free(json);
            free(keys);
            return NULL;
        }
        size_t key_len = strlen(escaped_key);

        /* Ensure capacity for key */
        while (pos + key_len + 256 >= capacity) {
            capacity *= 2;
            char* new_json = (char*)realloc(json, capacity);
            if (!new_json) {
                free(escaped_key);
                free(json);
                free(keys);
                return NULL;
            }
            json = new_json;
        }

        memcpy(json + pos, escaped_key, key_len);
        pos += key_len;
        free(escaped_key);

        json[pos++] = ':';

        /* Get value for this key */
        const cJSON* value = cJSON_GetObjectItemCaseSensitive(obj, keys[i]);

        /* Encode value recursively */
        char* encoded_value = encode_cjson_value(value);
        if (!encoded_value) {
            free(json);
            free(keys);
            return NULL;
        }
        size_t val_len = strlen(encoded_value);

        /* Ensure capacity for value */
        while (pos + val_len + 256 >= capacity) {
            capacity *= 2;
            char* new_json = (char*)realloc(json, capacity);
            if (!new_json) {
                free(encoded_value);
                free(json);
                free(keys);
                return NULL;
            }
            json = new_json;
        }

        memcpy(json + pos, encoded_value, val_len);
        pos += val_len;
        free(encoded_value);
    }

    json[pos++] = '}';
    json[pos] = '\0';

    free(keys);
    return json;
}

/* Encode a cJSON array with recursively sorted objects */
static char* encode_cjson_array(const cJSON* arr) {
    if (!cJSON_IsArray(arr)) return NULL;

    size_t capacity = INITIAL_JSON_CAPACITY;
    char* json = (char*)malloc(capacity);
    if (!json) return NULL;

    size_t pos = 0;
    json[pos++] = '[';

    int first = 1;
    const cJSON* item = arr->child;
    while (item) {
        if (!first) {
            json[pos++] = ',';
        }
        first = 0;

        char* encoded = encode_cjson_value(item);
        if (!encoded) {
            free(json);
            return NULL;
        }
        size_t val_len = strlen(encoded);

        /* Ensure capacity */
        while (pos + val_len + 256 >= capacity) {
            capacity *= 2;
            char* new_json = (char*)realloc(json, capacity);
            if (!new_json) {
                free(encoded);
                free(json);
                return NULL;
            }
            json = new_json;
        }

        memcpy(json + pos, encoded, val_len);
        pos += val_len;
        free(encoded);

        item = item->next;
    }

    json[pos++] = ']';
    json[pos] = '\0';

    return json;
}

/* Encode any cJSON value to string */
static char* encode_cjson_value(const cJSON* item) {
    if (!item) {
        char* null_str = (char*)malloc(5);
        if (null_str) strcpy(null_str, "null");
        return null_str;
    }

    if (cJSON_IsNull(item)) {
        char* null_str = (char*)malloc(5);
        if (null_str) strcpy(null_str, "null");
        return null_str;
    }

    if (cJSON_IsBool(item)) {
        if (cJSON_IsTrue(item)) {
            char* true_str = (char*)malloc(5);
            if (true_str) strcpy(true_str, "true");
            return true_str;
        } else {
            char* false_str = (char*)malloc(6);
            if (false_str) strcpy(false_str, "false");
            return false_str;
        }
    }

    if (cJSON_IsNumber(item)) {
        char* num_str = (char*)malloc(64);
        if (!num_str) return NULL;

        /* Check if it's an integer (no fractional part) */
        double val = item->valuedouble;
        if (val == (double)(long long)val && val >= -9007199254740992.0 && val <= 9007199254740992.0) {
            /* Integer - output without decimal point */
            snprintf(num_str, 64, "%lld", (long long)val);
        } else {
            /* Float - use %g to remove trailing zeros */
            snprintf(num_str, 64, "%g", val);
        }
        return num_str;
    }

    if (cJSON_IsString(item)) {
        return json_escape_string(item->valuestring);
    }

    if (cJSON_IsArray(item)) {
        return encode_cjson_array(item);
    }

    if (cJSON_IsObject(item)) {
        /* Recursively encode nested objects with sorted keys (no skip keys for nested) */
        return encode_cjson_object_sorted(item, NULL, 0);
    }

    /* Unknown type - return null */
    char* null_str = (char*)malloc(5);
    if (null_str) strcpy(null_str, "null");
    return null_str;
}

char* synclib_build_sorted_json_from_json(
    const char* input_json,
    const char** skip_keys,
    int skip_count
) {
    if (!input_json) return NULL;

    /* Parse input JSON */
    cJSON* root = cJSON_Parse(input_json);
    if (!root) {
        /* Parse error - return NULL */
        return NULL;
    }

    if (!cJSON_IsObject(root)) {
        /* Not an object - return as-is encoded */
        char* result = encode_cjson_value(root);
        cJSON_Delete(root);
        return result;
    }

    /* Build sorted JSON with skip keys */
    char* result = encode_cjson_object_sorted(root, skip_keys, skip_count);
    cJSON_Delete(root);
    return result;
}

char* synclib_row_hash_from_json(
    const char* row_id,
    const char* input_json,
    const char** skip_keys,
    int skip_count
) {
    if (!row_id || !input_json) return NULL;

    /* Build sorted JSON */
    char* sorted_json = synclib_build_sorted_json_from_json(input_json, skip_keys, skip_count);
    if (!sorted_json) return NULL;

    /* Compute row hash */
    char* hash = synclib_row_hash(row_id, sorted_json);
    free(sorted_json);

    return hash;
}

/* ========================================================================== */
/* Hashing Functions                                                          */
/* ========================================================================== */

char* synclib_sha256_hex(const char* data, size_t len) {
    char* hex = (char*)malloc(65);
    if (!hex) return NULL;

    sha256_hash_hex((const uint8_t*)data, len, hex);
    return hex;
}

char* synclib_row_hash(const char* row_id, const char* sorted_json) {
    if (!row_id || !sorted_json) return NULL;

    /* Build hash input: row_id|sorted_json */
    size_t id_len = strlen(row_id);
    size_t json_len = strlen(sorted_json);
    size_t input_len = id_len + 1 + json_len;

    char* input = (char*)malloc(input_len + 1);
    if (!input) return NULL;

    memcpy(input, row_id, id_len);
    input[id_len] = '|';
    memcpy(input + id_len + 1, sorted_json, json_len);
    input[input_len] = '\0';

    /* Compute hash */
    char* hash = synclib_sha256_hex(input, input_len);
    free(input);

    return hash;
}

char* synclib_row_hash_from_kvs(
    const char* row_id,
    const synclib_kv_t* kvs,
    int count,
    const char** skip_keys,
    int skip_count
) {
    char* json = synclib_build_sorted_json(kvs, count, skip_keys, skip_count);
    if (!json) return NULL;

    char* hash = synclib_row_hash(row_id, json);
    free(json);

    return hash;
}

char* synclib_block_hash(const char** row_hashes, int count) {
    if (count == 0) {
        /* Empty block - hash empty string */
        return synclib_sha256_hex("", 0);
    }

    /* Concatenate all row hashes (each is 64 chars) */
    size_t total_len = count * 64;
    char* combined = (char*)malloc(total_len + 1);
    if (!combined) return NULL;

    size_t pos = 0;
    for (int i = 0; i < count; i++) {
        if (!row_hashes[i] || strlen(row_hashes[i]) != 64) {
            free(combined);
            return NULL;
        }
        memcpy(combined + pos, row_hashes[i], 64);
        pos += 64;
    }
    combined[total_len] = '\0';

    /* Hash the combined string */
    char* hash = synclib_sha256_hex(combined, total_len);
    free(combined);

    return hash;
}

char* synclib_merkle_root(const char** block_hashes, int count) {
    if (count == 0) {
        /* Empty tree - return SHA256("") for consistency with server */
        return synclib_sha256_hex("", 0);
    }

    if (count == 1) {
        /* Single block - return as-is */
        char* result = (char*)malloc(65);
        if (!result) return NULL;
        memcpy(result, block_hashes[0], 65);
        return result;
    }

    /* Build tree level by level */
    int current_count = count;
    char** current_level = (char**)malloc(count * sizeof(char*));
    if (!current_level) return NULL;

    /* Copy initial hashes */
    for (int i = 0; i < count; i++) {
        current_level[i] = (char*)malloc(65);
        if (!current_level[i]) {
            for (int j = 0; j < i; j++) free(current_level[j]);
            free(current_level);
            return NULL;
        }
        memcpy(current_level[i], block_hashes[i], 65);
    }

    while (current_count > 1) {
        int next_count = (current_count + 1) / 2;
        char** next_level = (char**)malloc(next_count * sizeof(char*));
        if (!next_level) {
            for (int i = 0; i < current_count; i++) free(current_level[i]);
            free(current_level);
            return NULL;
        }

        int j = 0;
        for (int i = 0; i < current_count; i += 2) {
            if (i + 1 < current_count) {
                /* Pair exists - hash them together */
                char combined[129];
                memcpy(combined, current_level[i], 64);
                memcpy(combined + 64, current_level[i + 1], 64);
                combined[128] = '\0';

                next_level[j] = synclib_sha256_hex(combined, 128);
            } else {
                /* Odd node - pass up as-is */
                next_level[j] = (char*)malloc(65);
                if (next_level[j]) {
                    memcpy(next_level[j], current_level[i], 65);
                }
            }

            if (!next_level[j]) {
                for (int k = 0; k < j; k++) free(next_level[k]);
                free(next_level);
                for (int k = 0; k < current_count; k++) free(current_level[k]);
                free(current_level);
                return NULL;
            }
            j++;
        }

        /* Free current level */
        for (int i = 0; i < current_count; i++) free(current_level[i]);
        free(current_level);

        current_level = next_level;
        current_count = next_count;
    }

    /* Return the root */
    char* root = current_level[0];
    free(current_level);
    return root;
}

void synclib_free(void* ptr) {
    free(ptr);
}
