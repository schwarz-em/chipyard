#!/bin/sh
lli=${LLVMINTERP-lli}
exec $lli \
    /scratch/schwarzem/firesim-cs267/target-design/chipyard/generators/vadd_soc/tl0_vadd_tl_vadd/src/main/c/hls_prj/solution1/.autopilot/db/a.g.bc ${1+"$@"}
