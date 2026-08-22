#!/usr/bin/env bash
set -euo pipefail

: "${BENCHMARK_BINARY:?BENCHMARK_BINARY must be set}"

SEED=85740131
DEPTH=32
ORDERS=100000

echo "Generating benchmark input..."

python3 scripts/generate_matching_stress.py \
    --mode all \
    --depth "$DEPTH" \
    --seed "$SEED" \
    "$ORDERS" \
    > matching.txt

echo "Benchmarking $BENCHMARK_BINARY"

hyperfine \
    --warmup 1 \
    --runs 3 \
    "$BENCHMARK_BINARY < matching.txt > /dev/null 2>/dev/null"