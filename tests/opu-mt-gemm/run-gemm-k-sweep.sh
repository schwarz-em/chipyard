#!/usr/bin/env bash
#
# Sweep the K dimension of the multichip DMA GEMM (opu-multichip-dma-gemm) over
# powers of two, running each on UcieDmaDualIrisConfig in chipyard-chiplet/sims/vcs.
#
# Unlike the memcpy sweep (a -DDATA_LEN compile flag), K is baked into the
# generated dataset.h. So per size we: regenerate dataset.h with gendata.py,
# rebuild the binary, then run it via the same make invocation used by hand:
#
#   make run-binary CONFIG=UcieDmaDualIrisConfig USE_CHISEL7=1 \
#       BINARY=../../tests/build/opu-multichip-dma-gemm.riscv LOADMEM=1 \
#       EXTRA_SIM_FLAGS="+chip_id0=0x00004080:0x00000001 +chip_id1=0x00004080:0x00000002" \
#       TIMEOUT_CYCLES=5000000000
#
# M and N are held fixed (default 128). With N=128, K>1024 overflows the 128 KB
# TCM that stages matrix B, so the default sweep stops at 1024.
#
# Overridable via env: CHIPYARD_DIR, CONFIG, TIMEOUT_CYCLES, KS, MDIM, NDIM.

CHIPYARD_DIR="${CHIPYARD_DIR:-/scratch/schwarzem/chipyard-chiplet}"
GEMM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS_DIR="$CHIPYARD_DIR/tests"
SIM_DIR="$CHIPYARD_DIR/sims/vcs"
CONFIG="${CONFIG:-UcieDmaDualIrisConfig}"
TIMEOUT_CYCLES="${TIMEOUT_CYCLES:-5000000000}"
KS="${KS:-64 128 256 512 1024}"
MDIM="${MDIM:-128}"
NDIM="${NDIM:-128}"

# RISC-V toolchain + firtool (rebuild fallback) + conda's working cmake (the
# libidn workaround). Source the env BEFORE `set -u`: conda activation hooks
# reference unset vars and would abort under strict mode.
source "$CHIPYARD_DIR/env.sh" >/dev/null 2>&1
export PATH=/scratch/schwarzem/firtool/firtool-1.139.0/bin:$PATH
export PATH=/scratch/schwarzem/chipyard/.conda-env/bin:$PATH
set -u

DATASET="$GEMM_DIR/dataset.h"

# Preserve the checked-in dataset.h; restore it when the sweep finishes.
cp "$DATASET" "$DATASET.sweepbak"
restore_dataset() { [ -f "$DATASET.sweepbak" ] && mv -f "$DATASET.sweepbak" "$DATASET"; }
trap restore_dataset EXIT

# Make sure the cmake build tree is configured.
cmake -S "$TESTS_DIR" -B "$TESTS_DIR/build" -D CMAKE_BUILD_TYPE=Debug >/dev/null 2>&1

declare -a SUMMARY

for K in $KS; do
  echo "================ K=$K : regenerating dataset (M=$MDIM N=$NDIM K=$K) ================"
  if ! python3 "$GEMM_DIR/gendata.py" --mdim "$MDIM" --ndim "$NDIM" --kdim "$K" > "$DATASET"; then
    echo "  gendata.py failed for K=$K"
    SUMMARY+=("$K gendata-failed - -")
    continue
  fi

  echo "================ K=$K : rebuilding ================"
  if ! cmake --build "$TESTS_DIR/build" --target opu-multichip-dma-gemm 2>&1 | tail -3; then
    echo "  build failed for K=$K"
    SUMMARY+=("$K build-failed - -")
    continue
  fi

  echo "================ K=$K : running ================"
  log="$GEMM_DIR/k-sweep-$K.run.log"
  ( cd "$SIM_DIR" && make run-binary \
      CONFIG="$CONFIG" \
      USE_CHISEL7=1 \
      BINARY=../../tests/build/opu-multichip-dma-gemm.riscv \
      LOADMEM=1 \
      EXTRA_SIM_FLAGS="+chip_id0=0x00004080:0x00000001 +chip_id1=0x00004080:0x00000002" \
      TIMEOUT_CYCLES="$TIMEOUT_CYCLES" ) 2>&1 | tee "$log"

  # GEMM compute cycles (worker chip 2's timed pass) and the cross-chip DMA copy.
  gemm=$(grep -oE "Chip 2: [0-9]+ cycles" "$log" | grep -oE "[0-9]+" | head -1)
  dma=$(grep -oE "chiplet DMA memcpy [0-9]+ cycles" "$log" | grep -oE "[0-9]+" | head -1)
  if grep -q "SUCCESS;" "$log"; then status=PASS; else status=FAIL; fi
  SUMMARY+=("$K ${gemm:-N/A} ${dma:-N/A} $status")
done

echo
echo "======================== K SWEEP SUMMARY ========================"
printf "%-8s %-16s %-16s %-6s\n" "K_DIM" "gemm_cycles" "dma_copy_cycles" "status"
for row in "${SUMMARY[@]}"; do
  printf "%-8s %-16s %-16s %-6s\n" $row
done
