#!/bin/bash
# Build script for synclib_hash - cross-platform merkle hashing library
#
# Outputs:
#   - build/wasm/synclib_hash.js + synclib_hash.wasm (for web/Elixir)
#   - build/native/libsynclib_hash.a (static library for linking)

set -e

# Source emsdk if available
if [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
    export EMSDK_QUIET=1
    source "$HOME/emsdk/emsdk_env.sh" 2>/dev/null || true
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Building synclib_hash...${NC}"

# Create build directories
mkdir -p "$BUILD_DIR/wasm"
mkdir -p "$BUILD_DIR/native"

# Build native static library
build_native() {
    echo -e "${BLUE}Building native static library...${NC}"

    # Compile object files
    cc -c -O2 -fPIC "$SCRIPT_DIR/sha256.c" -o "$BUILD_DIR/native/sha256.o"
    cc -c -O2 -fPIC "$SCRIPT_DIR/hash.c" -o "$BUILD_DIR/native/hash.o"
    cc -c -O2 -fPIC "$SCRIPT_DIR/cJSON.c" -o "$BUILD_DIR/native/cJSON.o"

    # Create static library
    ar rcs "$BUILD_DIR/native/libsynclib_hash.a" \
        "$BUILD_DIR/native/sha256.o" \
        "$BUILD_DIR/native/hash.o" \
        "$BUILD_DIR/native/cJSON.o"

    echo -e "${GREEN}✓ Native library: $BUILD_DIR/native/libsynclib_hash.a${NC}"
}

# Build WebAssembly (for JavaScript/TypeScript - includes JS glue code)
build_wasm() {
    echo -e "${BLUE}Building WebAssembly (JS)...${NC}"

    # Check for emscripten
    if ! command -v emcc &> /dev/null; then
        echo -e "${RED}Error: emcc not found. Please install Emscripten.${NC}"
        echo "  brew install emscripten"
        echo "  # or"
        echo "  git clone https://github.com/emscripten-core/emsdk.git"
        echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
        exit 1
    fi

    emcc -O3 \
        "$SCRIPT_DIR/sha256.c" \
        "$SCRIPT_DIR/hash.c" \
        "$SCRIPT_DIR/cJSON.c" \
        -s WASM=1 \
        -s EXPORTED_FUNCTIONS='["_synclib_build_sorted_json","_synclib_build_sorted_json_from_json","_synclib_row_hash","_synclib_row_hash_from_json","_synclib_row_hash_from_kvs","_synclib_block_hash","_synclib_merkle_root","_synclib_sha256_hex","_synclib_free","_malloc","_free"]' \
        -s EXPORTED_RUNTIME_METHODS='["cwrap","ccall","getValue","setValue","UTF8ToString","stringToUTF8","lengthBytesUTF8"]' \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s MODULARIZE=1 \
        -s EXPORT_NAME='createSynclibHashModule' \
        -s ENVIRONMENT='web,node' \
        -s FILESYSTEM=0 \
        -s SINGLE_FILE=0 \
        -o "$BUILD_DIR/wasm/synclib_hash.js"

    echo -e "${GREEN}✓ WebAssembly module: $BUILD_DIR/wasm/synclib_hash.js${NC}"
    echo -e "${GREEN}✓ WebAssembly binary: $BUILD_DIR/wasm/synclib_hash.wasm${NC}"
}

# Build standalone WebAssembly (for Wasmex/Elixir - no JS dependencies)
build_wasm_standalone() {
    echo -e "${BLUE}Building standalone WebAssembly (Wasmex)...${NC}"

    if ! command -v emcc &> /dev/null; then
        echo -e "${RED}Error: emcc not found. Please install Emscripten.${NC}"
        return 1
    fi

    mkdir -p "$BUILD_DIR/wasm-standalone"

    # Build standalone WASM without JavaScript glue code
    # This is compatible with Wasmex (Elixir) and other WASM runtimes
    # Note: ALLOW_MEMORY_GROWTH=0 to avoid emscripten_notify_memory_growth import
    # 4MB should be plenty for hashing operations
    emcc -O3 \
        "$SCRIPT_DIR/sha256.c" \
        "$SCRIPT_DIR/hash.c" \
        "$SCRIPT_DIR/cJSON.c" \
        --no-entry \
        -s STANDALONE_WASM=1 \
        -s EXPORTED_FUNCTIONS='["_synclib_sha256_hex","_synclib_row_hash","_synclib_row_hash_from_json","_synclib_build_sorted_json_from_json","_synclib_block_hash","_synclib_merkle_root","_synclib_free","_malloc","_free"]' \
        -s ALLOW_MEMORY_GROWTH=0 \
        -s INITIAL_MEMORY=4194304 \
        -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
        -o "$BUILD_DIR/wasm-standalone/synclib_hash.wasm"

    echo -e "${GREEN}✓ Standalone WASM: $BUILD_DIR/wasm-standalone/synclib_hash.wasm${NC}"
}

# Build for macOS (universal binary)
build_macos() {
    echo -e "${BLUE}Building macOS universal library...${NC}"

    mkdir -p "$BUILD_DIR/macos"

    for ARCH in x86_64 arm64; do
        clang -arch $ARCH -c -O2 -fPIC "$SCRIPT_DIR/sha256.c" -o "$BUILD_DIR/macos/sha256_$ARCH.o"
        clang -arch $ARCH -c -O2 -fPIC "$SCRIPT_DIR/hash.c" -o "$BUILD_DIR/macos/hash_$ARCH.o"
        clang -arch $ARCH -c -O2 -fPIC "$SCRIPT_DIR/cJSON.c" -o "$BUILD_DIR/macos/cJSON_$ARCH.o"

        ar rcs "$BUILD_DIR/macos/libsynclib_hash_$ARCH.a" \
            "$BUILD_DIR/macos/sha256_$ARCH.o" \
            "$BUILD_DIR/macos/hash_$ARCH.o" \
            "$BUILD_DIR/macos/cJSON_$ARCH.o"
    done

    # Create universal library
    lipo -create \
        "$BUILD_DIR/macos/libsynclib_hash_x86_64.a" \
        "$BUILD_DIR/macos/libsynclib_hash_arm64.a" \
        -output "$BUILD_DIR/macos/libsynclib_hash.a"

    echo -e "${GREEN}✓ macOS universal library: $BUILD_DIR/macos/libsynclib_hash.a${NC}"
}

# Build for iOS (device + simulator)
build_ios() {
    echo -e "${BLUE}Building iOS libraries...${NC}"

    if ! command -v xcrun &> /dev/null; then
        echo -e "${RED}Error: xcrun not found. Please install Xcode.${NC}"
        return 1
    fi

    mkdir -p "$BUILD_DIR/ios"

    # iOS Device (arm64)
    echo "  Building iOS device (arm64)..."
    xcrun -sdk iphoneos clang -arch arm64 -c -O2 \
        -mios-version-min=12.0 \
        "$SCRIPT_DIR/sha256.c" -o "$BUILD_DIR/ios/sha256_device.o"
    xcrun -sdk iphoneos clang -arch arm64 -c -O2 \
        -mios-version-min=12.0 \
        "$SCRIPT_DIR/hash.c" -o "$BUILD_DIR/ios/hash_device.o"
    xcrun -sdk iphoneos clang -arch arm64 -c -O2 \
        -mios-version-min=12.0 \
        "$SCRIPT_DIR/cJSON.c" -o "$BUILD_DIR/ios/cJSON_device.o"
    ar rcs "$BUILD_DIR/ios/libsynclib_hash_device.a" \
        "$BUILD_DIR/ios/sha256_device.o" \
        "$BUILD_DIR/ios/hash_device.o" \
        "$BUILD_DIR/ios/cJSON_device.o"

    # iOS Simulator (arm64 + x86_64)
    for ARCH in arm64 x86_64; do
        echo "  Building iOS simulator ($ARCH)..."
        xcrun -sdk iphonesimulator clang -arch $ARCH -c -O2 \
            -mios-simulator-version-min=12.0 \
            "$SCRIPT_DIR/sha256.c" -o "$BUILD_DIR/ios/sha256_sim_$ARCH.o"
        xcrun -sdk iphonesimulator clang -arch $ARCH -c -O2 \
            -mios-simulator-version-min=12.0 \
            "$SCRIPT_DIR/hash.c" -o "$BUILD_DIR/ios/hash_sim_$ARCH.o"
        xcrun -sdk iphonesimulator clang -arch $ARCH -c -O2 \
            -mios-simulator-version-min=12.0 \
            "$SCRIPT_DIR/cJSON.c" -o "$BUILD_DIR/ios/cJSON_sim_$ARCH.o"
        ar rcs "$BUILD_DIR/ios/libsynclib_hash_sim_$ARCH.a" \
            "$BUILD_DIR/ios/sha256_sim_$ARCH.o" \
            "$BUILD_DIR/ios/hash_sim_$ARCH.o" \
            "$BUILD_DIR/ios/cJSON_sim_$ARCH.o"
    done

    # Create universal simulator library
    lipo -create \
        "$BUILD_DIR/ios/libsynclib_hash_sim_arm64.a" \
        "$BUILD_DIR/ios/libsynclib_hash_sim_x86_64.a" \
        -output "$BUILD_DIR/ios/libsynclib_hash_simulator.a"

    # Create XCFramework
    rm -rf "$BUILD_DIR/ios/synclib_hash.xcframework"
    xcodebuild -create-xcframework \
        -library "$BUILD_DIR/ios/libsynclib_hash_device.a" \
        -library "$BUILD_DIR/ios/libsynclib_hash_simulator.a" \
        -output "$BUILD_DIR/ios/synclib_hash.xcframework"

    # Also keep the simple static library for non-XCFramework use
    cp "$BUILD_DIR/ios/libsynclib_hash_device.a" "$BUILD_DIR/ios/libsynclib_hash.a"

    echo -e "${GREEN}✓ iOS XCFramework: $BUILD_DIR/ios/synclib_hash.xcframework${NC}"
    echo -e "${GREEN}✓ iOS static library: $BUILD_DIR/ios/libsynclib_hash.a${NC}"
}

# Build for Android (all architectures)
build_android() {
    echo -e "${BLUE}Building Android libraries...${NC}"

    # Check for NDK
    if [ -z "$ANDROID_NDK_HOME" ] && [ -z "$NDK_HOME" ]; then
        # Try common locations
        if [ -d "$HOME/Library/Android/sdk/ndk" ]; then
            NDK_ROOT=$(ls -d "$HOME/Library/Android/sdk/ndk"/*/ 2>/dev/null | sort -V | tail -1)
        elif [ -d "/usr/local/lib/android/sdk/ndk" ]; then
            NDK_ROOT=$(ls -d "/usr/local/lib/android/sdk/ndk"/*/ 2>/dev/null | sort -V | tail -1)
        fi
    else
        NDK_ROOT="${ANDROID_NDK_HOME:-$NDK_HOME}"
    fi

    if [ -z "$NDK_ROOT" ] || [ ! -d "$NDK_ROOT" ]; then
        echo -e "${RED}Error: Android NDK not found.${NC}"
        echo "  Set ANDROID_NDK_HOME or NDK_HOME environment variable"
        return 1
    fi

    echo "  Using NDK: $NDK_ROOT"

    TOOLCHAIN="$NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64"
    if [ ! -d "$TOOLCHAIN" ]; then
        TOOLCHAIN="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
    fi

    API_LEVEL=21

    # Build for each architecture
    build_android_arch() {
        local ARCH=$1
        local TARGET=$2
        echo "  Building Android $ARCH..."

        mkdir -p "$BUILD_DIR/android/$ARCH"

        CC="$TOOLCHAIN/bin/${TARGET}${API_LEVEL}-clang"

        "$CC" -c -O2 -fPIC \
            "$SCRIPT_DIR/sha256.c" -o "$BUILD_DIR/android/$ARCH/sha256.o"
        "$CC" -c -O2 -fPIC \
            "$SCRIPT_DIR/hash.c" -o "$BUILD_DIR/android/$ARCH/hash.o"
        "$CC" -c -O2 -fPIC \
            "$SCRIPT_DIR/cJSON.c" -o "$BUILD_DIR/android/$ARCH/cJSON.o"

        "$TOOLCHAIN/bin/llvm-ar" rcs "$BUILD_DIR/android/$ARCH/libsynclib_hash.a" \
            "$BUILD_DIR/android/$ARCH/sha256.o" \
            "$BUILD_DIR/android/$ARCH/hash.o" \
            "$BUILD_DIR/android/$ARCH/cJSON.o"
    }

    build_android_arch "arm64-v8a" "aarch64-linux-android"
    build_android_arch "armeabi-v7a" "armv7a-linux-androideabi"
    build_android_arch "x86" "i686-linux-android"
    build_android_arch "x86_64" "x86_64-linux-android"

    echo -e "${GREEN}✓ Android libraries built for: arm64-v8a armeabi-v7a x86 x86_64${NC}"
}

# Run tests
run_tests() {
    echo -e "${BLUE}Running tests...${NC}"

    # Compile test program
    cc -O2 "$SCRIPT_DIR/test_hash.c" \
        "$SCRIPT_DIR/sha256.c" \
        "$SCRIPT_DIR/hash.c" \
        "$SCRIPT_DIR/cJSON.c" \
        -o "$BUILD_DIR/test_hash"

    # Run tests
    "$BUILD_DIR/test_hash"
}

# Clean build directory
clean() {
    echo -e "${BLUE}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}✓ Clean complete${NC}"
}

# Print usage
usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  native          Build native static library"
    echo "  wasm            Build WebAssembly module (JS)"
    echo "  wasm-standalone Build standalone WASM (Wasmex/Elixir)"
    echo "  macos           Build macOS universal library"
    echo "  ios       Build iOS libraries (device + simulator)"
    echo "  android   Build Android libraries (all architectures)"
    echo "  test      Build and run tests"
    echo "  all       Build everything"
    echo "  clean     Clean build directory"
    echo ""
    echo "Default: all"
}

# Main
case "${1:-all}" in
    native)
        build_native
        ;;
    wasm)
        build_wasm
        ;;
    wasm-standalone)
        build_wasm_standalone
        ;;
    macos)
        build_macos
        ;;
    ios)
        build_ios
        ;;
    android)
        build_android
        ;;
    test)
        run_tests
        ;;
    all)
        build_native
        build_macos
        build_ios
        build_android
        build_wasm
        build_wasm_standalone
        ;;
    clean)
        clean
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        echo -e "${RED}Unknown command: $1${NC}"
        usage
        exit 1
        ;;
esac
