#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export ANDROID_HOME="${ANDROID_HOME:-/opt/android-sdk}"
GRADLE_BIN="${GRADLE_BIN:-/opt/gradle/gradle-8.7/bin/gradle}"
ZIPALIGN_BIN="$ANDROID_HOME/build-tools/34.0.0/zipalign"
APKSIGNER_BIN="$ANDROID_HOME/build-tools/34.0.0/apksigner"

cd "$SCRIPT_DIR"

echo "[1/4] Running Host C++20 Verification Test Suite..."
mkdir -p build-host
cmake -S app/src/main/cpp -B build-host -GNinja
cmake --build build-host
./build-host/population_misfits_tests

echo "[2/4] Building arm64-v8a Android OpenXR Release APK..."
"$GRADLE_BIN" assembleRelease --no-daemon

UNSIGNED_APK="$SCRIPT_DIR/app/build/outputs/apk/release/app-release-unsigned.apk"
ALIGNED_APK="/tmp/population-misfits-aligned.apk"
SIGNED_APK="${1:-$SCRIPT_DIR/build/population-misfits-v1.apk}"
KEYSTORE="/tmp/misfits-release.keystore"

mkdir -p "$(dirname "$SIGNED_APK")"

if [ ! -f "$KEYSTORE" ]; then
    keytool -genkeypair -v \
        -keystore "$KEYSTORE" \
        -alias misfits_vr \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -storepass misfits123 -keypass misfits123 \
        -dname "CN=Population Misfits VR, OU=VR Studio, O=Population Misfits, L=Seattle, S=WA, C=US"
fi

echo "[3/4] Running zipalign (4-byte page alignment)..."
rm -f "$ALIGNED_APK" "$SIGNED_APK"
"$ZIPALIGN_BIN" -p 4 "$UNSIGNED_APK" "$ALIGNED_APK"

echo "[4/4] Signing APK (v1 + v2 + v3 schemes) and verifying..."
"$APKSIGNER_BIN" sign \
    --ks "$KEYSTORE" \
    --ks-key-alias misfits_vr \
    --ks-pass pass:misfits123 \
    --key-pass pass:misfits123 \
    --min-sdk-version 21 \
    --v1-signing-enabled true \
    --v2-signing-enabled true \
    --v3-signing-enabled true \
    --out "$SIGNED_APK" \
    "$ALIGNED_APK"

"$APKSIGNER_BIN" verify --min-sdk-version 21 --verbose "$SIGNED_APK"
echo "Built and verified APK at: $SIGNED_APK"
