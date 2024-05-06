# 1 "/scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd_tl/accel.c"
# 1 "/scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd_tl/accel.c" 1
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 149 "<built-in>" 3
# 1 "<command line>" 1





# 1 "/ecad/tools/xilinx/Vivado/2018.2/common/technology/autopilot/etc/autopilot_ssdm_op.h" 1
# 300 "/ecad/tools/xilinx/Vivado/2018.2/common/technology/autopilot/etc/autopilot_ssdm_op.h"
    void _ssdm_op_IfRead() __attribute__ ((nothrow));
    void _ssdm_op_IfWrite() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_op_IfNbRead() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_op_IfNbWrite() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_op_IfCanRead() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_op_IfCanWrite() __attribute__ ((nothrow));


    void _ssdm_StreamRead() __attribute__ ((nothrow));
    void _ssdm_StreamWrite() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_StreamNbRead() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_StreamNbWrite() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_StreamCanRead() __attribute__ ((nothrow));
    unsigned int __attribute__ ((bitwidth(1))) _ssdm_StreamCanWrite() __attribute__ ((nothrow));




    void _ssdm_op_MemShiftRead() __attribute__ ((nothrow));

    void _ssdm_op_Wait() __attribute__ ((nothrow));
    void _ssdm_op_Poll() __attribute__ ((nothrow));

    void _ssdm_op_Return() __attribute__ ((nothrow));


    void _ssdm_op_SpecSynModule() __attribute__ ((nothrow));
    void _ssdm_op_SpecTopModule() __attribute__ ((nothrow));
    void _ssdm_op_SpecProcessDecl() __attribute__ ((nothrow));
    void _ssdm_op_SpecProcessDef() __attribute__ ((nothrow));
    void _ssdm_op_SpecPort() __attribute__ ((nothrow));
    void _ssdm_op_SpecConnection() __attribute__ ((nothrow));
    void _ssdm_op_SpecChannel() __attribute__ ((nothrow));
    void _ssdm_op_SpecSensitive() __attribute__ ((nothrow));
    void _ssdm_op_SpecModuleInst() __attribute__ ((nothrow));
    void _ssdm_op_SpecPortMap() __attribute__ ((nothrow));

    void _ssdm_op_SpecReset() __attribute__ ((nothrow));

    void _ssdm_op_SpecPlatform() __attribute__ ((nothrow));
    void _ssdm_op_SpecClockDomain() __attribute__ ((nothrow));
    void _ssdm_op_SpecPowerDomain() __attribute__ ((nothrow));

    int _ssdm_op_SpecRegionBegin() __attribute__ ((nothrow));
    int _ssdm_op_SpecRegionEnd() __attribute__ ((nothrow));

    void _ssdm_op_SpecLoopName() __attribute__ ((nothrow));

    void _ssdm_op_SpecLoopTripCount() __attribute__ ((nothrow));

    int _ssdm_op_SpecStateBegin() __attribute__ ((nothrow));
    int _ssdm_op_SpecStateEnd() __attribute__ ((nothrow));

    void _ssdm_op_SpecInterface() __attribute__ ((nothrow));

    void _ssdm_op_SpecPipeline() __attribute__ ((nothrow));
    void _ssdm_op_SpecDataflowPipeline() __attribute__ ((nothrow));


    void _ssdm_op_SpecLatency() __attribute__ ((nothrow));
    void _ssdm_op_SpecParallel() __attribute__ ((nothrow));
    void _ssdm_op_SpecProtocol() __attribute__ ((nothrow));
    void _ssdm_op_SpecOccurrence() __attribute__ ((nothrow));

    void _ssdm_op_SpecResource() __attribute__ ((nothrow));
    void _ssdm_op_SpecResourceLimit() __attribute__ ((nothrow));
    void _ssdm_op_SpecCHCore() __attribute__ ((nothrow));
    void _ssdm_op_SpecFUCore() __attribute__ ((nothrow));
    void _ssdm_op_SpecIFCore() __attribute__ ((nothrow));
    void _ssdm_op_SpecIPCore() __attribute__ ((nothrow));
    void _ssdm_op_SpecKeepValue() __attribute__ ((nothrow));
    void _ssdm_op_SpecMemCore() __attribute__ ((nothrow));

    void _ssdm_op_SpecExt() __attribute__ ((nothrow));




    void _ssdm_SpecArrayDimSize() __attribute__ ((nothrow));

    void _ssdm_RegionBegin() __attribute__ ((nothrow));
    void _ssdm_RegionEnd() __attribute__ ((nothrow));

    void _ssdm_Unroll() __attribute__ ((nothrow));
    void _ssdm_UnrollRegion() __attribute__ ((nothrow));

    void _ssdm_InlineAll() __attribute__ ((nothrow));
    void _ssdm_InlineLoop() __attribute__ ((nothrow));
    void _ssdm_Inline() __attribute__ ((nothrow));
    void _ssdm_InlineSelf() __attribute__ ((nothrow));
    void _ssdm_InlineRegion() __attribute__ ((nothrow));

    void _ssdm_SpecArrayMap() __attribute__ ((nothrow));
    void _ssdm_SpecArrayPartition() __attribute__ ((nothrow));
    void _ssdm_SpecArrayReshape() __attribute__ ((nothrow));

    void _ssdm_SpecStream() __attribute__ ((nothrow));

    void _ssdm_SpecExpr() __attribute__ ((nothrow));
    void _ssdm_SpecExprBalance() __attribute__ ((nothrow));

    void _ssdm_SpecDependence() __attribute__ ((nothrow));

    void _ssdm_SpecLoopMerge() __attribute__ ((nothrow));
    void _ssdm_SpecLoopFlatten() __attribute__ ((nothrow));
    void _ssdm_SpecLoopRewind() __attribute__ ((nothrow));

    void _ssdm_SpecFuncInstantiation() __attribute__ ((nothrow));
    void _ssdm_SpecFuncBuffer() __attribute__ ((nothrow));
    void _ssdm_SpecFuncExtract() __attribute__ ((nothrow));
    void _ssdm_SpecConstant() __attribute__ ((nothrow));

    void _ssdm_DataPack() __attribute__ ((nothrow));
    void _ssdm_SpecDataPack() __attribute__ ((nothrow));

    void _ssdm_op_SpecBitsMap() __attribute__ ((nothrow));
    void _ssdm_op_SpecLicense() __attribute__ ((nothrow));
# 7 "<command line>" 2
# 1 "<built-in>" 2
# 1 "/scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd_tl/accel.c" 2
# 1 "/scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd_tl/accel.h" 1
# 10 "/scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd_tl/accel.h"
int vadd(int * a, int * b, int* c, int length);
# 2 "/scratch/schwarzem/firesim-cs267/target-design/chipyard/tools/centrifuge/examples/vadd_proj/src/hls/vadd_tl/accel.c" 2

int vadd(int * a, int * b, int* c, int length) {


_ssdm_op_SpecInterface(a, "m_axi", 0, 0, "", 125, 16, "gmem0", "slave", "", 16, 16, 16, 16, "", "");

_ssdm_op_SpecInterface(b, "m_axi", 0, 0, "", 125, 16, "gmem0", "slave", "", 16, 16, 16, 16, "", "");

_ssdm_op_SpecInterface(c, "m_axi", 0, 0, "", 125, 16, "gmem0", "slave", "", 16, 16, 16, 16, "", "");


_ssdm_op_SpecInterface(a, "s_axilite", 0, 0, "", 0, 0, "control", "", "", 0, 0, 0, 0, "", "");
_ssdm_op_SpecInterface(b, "s_axilite", 0, 0, "", 0, 0, "control", "", "", 0, 0, 0, 0, "", "");
_ssdm_op_SpecInterface(c, "s_axilite", 0, 0, "", 0, 0, "control", "", "", 0, 0, 0, 0, "", "");
_ssdm_op_SpecInterface(length, "s_axilite", 0, 0, "", 0, 0, "control", "", "", 0, 0, 0, 0, "", "");
_ssdm_op_SpecInterface(0, "s_axilite", 0, 0, "", 0, 0, "control", "", "", 0, 0, 0, 0, "", "");


 int upper = (length >> 3) << 3;
    int i = 0;
    for (i = 0; i < upper; i += 8) {
        c[i+0] = a[i+0] +b[i+0];
        c[i+1] = a[i+1] +b[i+1];
        c[i+2] = a[i+2] +b[i+2];
        c[i+3] = a[i+3] +b[i+3];

        c[i+4] = a[i+4] +b[i+4];
        c[i+5] = a[i+5] +b[i+5];
        c[i+6] = a[i+6] +b[i+6];
        c[i+7] = a[i+7] +b[i+7];
    }

    for (i = upper; i < length; i++) {
        c[i] = a[i] +b[i];
    }
    return 0;
}
