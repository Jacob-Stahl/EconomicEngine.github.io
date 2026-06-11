#!/bin/bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"

cd "$PROJECT_ROOT"

if ! command -v emcc >/dev/null 2>&1; then
    echo "emcc not found. Install and activate the Emscripten SDK before building." >&2
    exit 1
fi

if ! command -v node >/dev/null 2>&1; then
    echo "node not found. Emscripten uses node when generating TypeScript declarations." >&2
    exit 1
fi

OUT_DIR="$PROJECT_ROOT/webdemo"
SVELTE_OUT_DIR="$PROJECT_ROOT/webdemo-svelte/src/lib/generated"

mkdir -p "$OUT_DIR"

echo "Compiling eelib to WebAssembly ES module..."

emcc -O3 \
    eelib/wasm_bindings.cpp \
    eelib/agents/abm.cpp \
    eelib/agents/agent.cpp \
    eelib/agents/agent_manager.cpp \
    eelib/agents/consumer.cpp \
    eelib/agents/producer.cpp \
    eelib/agents/manufacturer.cpp \
    eelib/agents/person.cpp \
    eelib/limits_bin.cpp \
    eelib/matcher.cpp \
    eelib/notifier.cpp \
    eelib/order.cpp \
    -I eelib \
    -std=c++26 \
    -lembind \
    -sWASM=1 \
    -sMODULARIZE=1 \
    -sEXPORT_ES6=1 \
    -sEXPORT_NAME=createEelib \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web \
    --emit-tsd "$OUT_DIR/eelib.d.ts" \
    -o "$OUT_DIR/eelib.mjs"

echo "Build complete!"
echo "Generated:"
echo "  $OUT_DIR/eelib.mjs"
echo "  $OUT_DIR/eelib.wasm"
echo "  $OUT_DIR/eelib.d.ts"

if [[ -d "$SVELTE_OUT_DIR" ]]; then
    cp "$OUT_DIR/eelib.mjs" "$SVELTE_OUT_DIR/eelib.mjs"
    cp "$OUT_DIR/eelib.wasm" "$SVELTE_OUT_DIR/eelib.wasm"
    cp "$OUT_DIR/eelib.d.ts" "$SVELTE_OUT_DIR/eelib.d.ts"

    echo "Synced Svelte artifacts:"
    echo "  $SVELTE_OUT_DIR/eelib.mjs"
    echo "  $SVELTE_OUT_DIR/eelib.wasm"
    echo "  $SVELTE_OUT_DIR/eelib.d.ts"
fi