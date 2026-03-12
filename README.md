# synclib_hash

Cross-platform Merkle tree hashing library. Single source of truth for consistent hashing across all platforms.

## Purpose

This library ensures that Merkle tree hashes are computed identically across:
- Native platforms (iOS, Android, macOS, Linux, Windows) via C/FFI
- Web browsers via WebAssembly
- Server (Elixir) via WebAssembly + Wasmex
- TypeScript/JavaScript via WebAssembly

## Hash Format

- **Row hash**: `SHA256(row_id + "|" + sorted_json(row_data))` → lowercase hex
- **Block hash**: `SHA256(concat of row hash hex strings)` → lowercase hex
- **Merkle root**: Binary tree of block hashes, odd nodes passed up as-is

## Building

```bash
# Build everything (native, macOS, iOS, Android, WASM)
./build.sh all

# Build specific platforms
./build.sh native          # Native static library
./build.sh macos           # macOS universal library (x86_64 + arm64)
./build.sh ios             # iOS device + simulator (XCFramework)
./build.sh android         # Android all architectures
./build.sh wasm            # WebAssembly (JS loader + .wasm)
./build.sh wasm-standalone # Standalone WASM (for Wasmex/Elixir)

# Run tests
./build.sh test

# Clean
./build.sh clean
```

### Prerequisites

| Platform | Requirements |
|----------|--------------|
| **native/macOS** | Xcode Command Line Tools (`clang`, `ar`, `lipo`) |
| **iOS** | Xcode with iOS SDK (`xcrun`) |
| **Android** | Android NDK — set `ANDROID_NDK_HOME` or `NDK_HOME` |
| **WASM** | Emscripten SDK (`emcc`) — [install guide](https://emscripten.org/docs/getting_started/downloads.html) |

## Output

```
build/
  native/
    libsynclib_hash.a         # Native static library
  macos/
    libsynclib_hash.a         # macOS universal (x86_64 + arm64)
  ios/
    synclib_hash.xcframework/ # iOS XCFramework
    libsynclib_hash.a         # iOS device static library
  android/
    arm64-v8a/libsynclib_hash.a
    armeabi-v7a/libsynclib_hash.a
    x86/libsynclib_hash.a
    x86_64/libsynclib_hash.a
  wasm/
    synclib_hash.js           # WASM loader
    synclib_hash.wasm         # WebAssembly binary (~25KB)
```

## API

```c
// Build sorted JSON from key-value pairs
char* synclib_build_sorted_json(
    const synclib_kv_t* kvs,
    int count,
    const char** skip_keys,
    int skip_count
);

// Compute row hash
char* synclib_row_hash(const char* row_id, const char* sorted_json);

// Compute block hash from row hashes
char* synclib_block_hash(const char** row_hashes, int count);

// Build merkle root from block hashes
char* synclib_merkle_root(const char** block_hashes, int count);

// Free allocated strings
void synclib_free(void* ptr);
```

## Integration

### With synclibc

Link against `libsynclib_hash.a` and include `hash.h`:

```c
#include "synclib_hash/hash.h"
```

### With Elixir (via Wasmex)

```elixir
{:ok, bytes} = File.read("synclib_hash.wasm")
{:ok, module} = Wasmex.Module.compile(bytes)
{:ok, instance} = Wasmex.Instance.new(module)

# Call functions via Wasmex
```

### With TypeScript/JavaScript

```typescript
import createSynclibHashModule from './synclib_hash.js';

const module = await createSynclibHashModule();

const rowHash = module.ccall(
  'synclib_row_hash',
  'number',
  ['string', 'string'],
  [rowId, sortedJson]
);
```

## Test Vectors

The `test_vectors.json` file contains canonical test cases for verifying cross-platform consistency. Use these vectors to validate your platform-specific implementation produces identical hashes.

Key test cases:
- SHA256 hashing
- Sorted JSON building
- Row hash computation
- Block hash computation
- Merkle root building (including odd-node handling)
- End-to-end scenario with 3 rows

## Files

- `hash.h` - Public API header
- `hash.c` - Implementation (sorted JSON, hashing, merkle tree)
- `sha256.h` - SHA256 header
- `sha256.c` - SHA256 implementation
- `test_hash.c` - Test suite
- `test_vectors.json` - Cross-platform test vectors
- `build.sh` - Build script
