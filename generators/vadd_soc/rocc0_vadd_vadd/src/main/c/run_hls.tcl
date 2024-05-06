open_project -reset hls_prj
set_top vadd
add_files /scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd/vadd.c
add_files /scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd/accel.c
add_files /scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd/accel.h
open_solution -reset "solution1"
set_part {xcvu9p-flgb2104-2-i}
config_compile -ignore_long_run_time
create_clock -period 10 -name default
config_rtl -prefix rocc0_vadd_
config_interface -m_axi_addr64
#export_design -format ip_catalog
#config_interface -clock_enable
#source "./directives.tcl"
csynth_design
exit
