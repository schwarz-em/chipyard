#!/usr/bin/env bash
#
# Sweep the K dimension of the multichip Saturn-DMA GEMM (opu-multichip-saturn-dma-gemm)
# over powers of two, running each on UcieSaturnDMADualIrisConfig in
# chipyard-chiplet/sims/vcs.
#
# Unlike run-gemm-k-sweep.sh / run-gemm-mempress-k-sweep.sh, the cross-chip
# copy here isn't a RoCC/MMIO accelerator hart 0 can just command inline --
# it's a second hart (the Saturn DMA-only rocket core added by
# UcieSaturnDMASinglecoreIrisConfig). Hart 0 (shuttle/OPU) runs the GEMM,
# hands off to DMA_HART_ID via a chip-local flag, and parks; DMA_HART_ID does
# the actual vector-copy across UCIe and is the one that signals success for
# that chip. See multichip-saturn-dma-main.c for the handoff.
#
# K is baked into the generated dataset.h, so per size we: regenerate
# dataset.h with gendata.py, rebuild the binary, then run it via the same
# make invocation used by hand:
#
#   make run-binary CONFIG=UcieSaturnDMADualIrisConfig USE_CHISEL7=1 \
#       BINARY=../../tests/build/opu-multichip-saturn-dma-gemm.riscv LOADMEM=1 \
#       EXTRA_SIM_FLAGS="+chip_id0=0x00004080:0x00000001 +chip_id1=0x00004080:0x00000002" \
#       TIMEOUT_CYCLES=5000000000
#
# M and N are held fixed (default 128). With N=128, K>1024 overflows the 128 KB
# TCM that stages matrix B, so the default sweep stops at 1024.
#
# DMA_HART_ID (which hart is the Saturn DMA core) is a compile-time default
# in multichip-saturn-dma-main.c, not a sweep knob -- it's fixed by which
# rocket tile UcieSaturnDMASinglecoreIrisConfig actually builds as the DMA
# core (hart 1). Edit that #define (or add a target_compile_definitions
# override in CMakeLists.txt) if you need a different value.
#
# Overridable via env: CHIPYARD_DIR, CONFIG, TIMEOUT_CYCLES, KS, MDIM, NDIM.

CHIPYARD_DIR="${CHIPYARD_DIR:-/scratch/schwarzem/chipyard-chiplet}"
GEMM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS_DIR="$CHIPYARD_DIR/tests"
SIM_DIR="$CHIPYARD_DIR/sims/vcs"
CONFIG="${CONFIG:-UcieSaturnDMADualIrisConfig}"
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
    SUMMARY+=("$K gendata-failed - - -")
    continue
  fi

  echo "================ K=$K : rebuilding ================"
  if ! cmake --build "$TESTS_DIR/build" --target opu-multichip-saturn-dma-gemm 2>&1 | tail -3; then
    echo "  build failed for K=$K"
    SUMMARY+=("$K build-failed - - -")
    continue
  fi

  echo "================ K=$K : running ================"
  log="$GEMM_DIR/k-sweep-saturn-dma-$K.run.log"
  ( cd "$SIM_DIR" && make run-binary \
      CONFIG="$CONFIG" \
      USE_CHISEL7=1 \
      BINARY=../../tests/build/opu-multichip-saturn-dma-gemm.riscv \
      LOADMEM=1 \
      EXTRA_SIM_FLAGS="+chip_id0=0x00004080:0x00000001 +chip_id1=0x00004080:0x00000002" \
      TIMEOUT_CYCLES="$TIMEOUT_CYCLES" ) 2>&1 | tee "$log"

  # GEMM compute cycles for each chip's own timed pass (chip 1 is the
  # verifier, chip 2 the worker -- both compute their own M-slice) and the
  # cross-chip Saturn DMA copy.
  gemm1=$(grep -oE "Chip 1: [0-9]+ cycles" "$log" | grep -oE "[0-9]+" | head -1)
  gemm2=$(grep -oE "Chip 2: [0-9]+ cycles" "$log" | grep -oE "[0-9]+" | head -1)
  dma=$(grep -oE "chiplet Saturn DMA memcpy [0-9]+ cycles" "$log" | grep -oE "[0-9]+" | head -1)
  if grep -q "SUCCESS;" "$log"; then status=PASS; else status=FAIL; fi
  SUMMARY+=("$K ${gemm1:-N/A} ${gemm2:-N/A} ${dma:-N/A} $status")
done

echo
echo "======================== K SWEEP SUMMARY ========================"
printf "%-8s %-16s %-16s %-16s %-6s\n" "K_DIM" "gemm_chip1_cyc" "gemm_chip2_cyc" "dma_copy_cycles" "status"
for row in "${SUMMARY[@]}"; do
  printf "%-8s %-16s %-16s %-16s %-6s\n" $row
done
