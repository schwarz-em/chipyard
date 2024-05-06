#!/bin/sh
lli=${LLVMINTERP-lli}
exec $lli \
    /scratch/schwarzem/firesim-cs267/target-design/chipyard/generators/vadd_soc/rocc0_vadd_vadd/src/main/c/hls_prj/solution1/.autopilot/db/a.g.bc ${1+"$@"}
