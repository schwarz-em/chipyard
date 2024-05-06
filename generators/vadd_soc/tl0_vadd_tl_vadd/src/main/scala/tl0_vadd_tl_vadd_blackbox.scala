package hls_tl0_vadd_tl_vadd

import sys.process._

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config.{Parameters, Field}
import freechips.rocketchip.tile._
import freechips.rocketchip.util._

class tl0_vadd_tl_vadd() extends BlackBox() with HasBlackBoxPath {
    val C_S_AXI_CONTROL_DATA_WIDTH = 32
    val C_S_AXI_CONTROL_ADDR_WIDTH = 7
    val C_S_AXI_DATA_WIDTH = 32
    val C_M_AXI_GMEM0_ID_WIDTH = 1
    val C_M_AXI_GMEM0_ADDR_WIDTH = 64
    val C_M_AXI_GMEM0_DATA_WIDTH = 32
    val C_M_AXI_GMEM0_AWUSER_WIDTH = 1
    val C_M_AXI_GMEM0_ARUSER_WIDTH = 1
    val C_M_AXI_GMEM0_WUSER_WIDTH = 1
    val C_M_AXI_GMEM0_RUSER_WIDTH = 1
    val C_M_AXI_GMEM0_BUSER_WIDTH = 1
    val C_M_AXI_GMEM0_USER_VALUE = 0
    val C_M_AXI_GMEM0_PROT_VALUE = 0
    val C_M_AXI_GMEM0_CACHE_VALUE = 3
    val C_M_AXI_DATA_WIDTH = 32
    val C_S_AXI_CONTROL_WSTRB_WIDTH = (32 / 8)
    val C_S_AXI_WSTRB_WIDTH = (32 / 8)
    val C_M_AXI_GMEM0_WSTRB_WIDTH = (32 / 8)
    val C_M_AXI_WSTRB_WIDTH = (32 / 8)
    val io = IO(new Bundle {
        val ap_clk = Input(Clock())
        val ap_rst_n = Input(Bool())
        val m_axi_gmem0_AWREADY = Input(Bool())
        val m_axi_gmem0_WREADY = Input(Bool())
        val m_axi_gmem0_ARREADY = Input(Bool())
        val m_axi_gmem0_RVALID = Input(Bool())
        val m_axi_gmem0_RDATA = Input(UInt(C_M_AXI_GMEM0_DATA_WIDTH.W))
        val m_axi_gmem0_RLAST = Input(Bool())
        val m_axi_gmem0_RID = Input(UInt(C_M_AXI_GMEM0_ID_WIDTH.W))
        val m_axi_gmem0_RUSER = Input(UInt(C_M_AXI_GMEM0_RUSER_WIDTH.W))
        val m_axi_gmem0_RRESP = Input(UInt(2.W))
        val m_axi_gmem0_BVALID = Input(Bool())
        val m_axi_gmem0_BRESP = Input(UInt(2.W))
        val m_axi_gmem0_BID = Input(UInt(C_M_AXI_GMEM0_ID_WIDTH.W))
        val m_axi_gmem0_BUSER = Input(UInt(C_M_AXI_GMEM0_BUSER_WIDTH.W))
        val s_axi_control_AWVALID = Input(Bool())
        val s_axi_control_AWADDR = Input(UInt(C_S_AXI_CONTROL_ADDR_WIDTH.W))
        val s_axi_control_WVALID = Input(Bool())
        val s_axi_control_WDATA = Input(UInt(C_S_AXI_CONTROL_DATA_WIDTH.W))
        val s_axi_control_WSTRB = Input(UInt(C_S_AXI_CONTROL_WSTRB_WIDTH.W))
        val s_axi_control_ARVALID = Input(Bool())
        val s_axi_control_ARADDR = Input(UInt(C_S_AXI_CONTROL_ADDR_WIDTH.W))
        val s_axi_control_RREADY = Input(Bool())
        val s_axi_control_BREADY = Input(Bool())
        val m_axi_gmem0_AWVALID = Output(Bool())
        val m_axi_gmem0_AWADDR = Output(UInt(C_M_AXI_GMEM0_ADDR_WIDTH.W))
        val m_axi_gmem0_AWID = Output(UInt(C_M_AXI_GMEM0_ID_WIDTH.W))
        val m_axi_gmem0_AWLEN = Output(UInt(8.W))
        val m_axi_gmem0_AWSIZE = Output(UInt(3.W))
        val m_axi_gmem0_AWBURST = Output(UInt(2.W))
        val m_axi_gmem0_AWLOCK = Output(UInt(2.W))
        val m_axi_gmem0_AWCACHE = Output(UInt(4.W))
        val m_axi_gmem0_AWPROT = Output(UInt(3.W))
        val m_axi_gmem0_AWQOS = Output(UInt(4.W))
        val m_axi_gmem0_AWREGION = Output(UInt(4.W))
        val m_axi_gmem0_AWUSER = Output(UInt(C_M_AXI_GMEM0_AWUSER_WIDTH.W))
        val m_axi_gmem0_WVALID = Output(Bool())
        val m_axi_gmem0_WDATA = Output(UInt(C_M_AXI_GMEM0_DATA_WIDTH.W))
        val m_axi_gmem0_WSTRB = Output(UInt(C_M_AXI_GMEM0_WSTRB_WIDTH.W))
        val m_axi_gmem0_WLAST = Output(Bool())
        val m_axi_gmem0_WID = Output(UInt(C_M_AXI_GMEM0_ID_WIDTH.W))
        val m_axi_gmem0_WUSER = Output(UInt(C_M_AXI_GMEM0_WUSER_WIDTH.W))
        val m_axi_gmem0_ARVALID = Output(Bool())
        val m_axi_gmem0_ARADDR = Output(UInt(C_M_AXI_GMEM0_ADDR_WIDTH.W))
        val m_axi_gmem0_ARID = Output(UInt(C_M_AXI_GMEM0_ID_WIDTH.W))
        val m_axi_gmem0_ARLEN = Output(UInt(8.W))
        val m_axi_gmem0_ARSIZE = Output(UInt(3.W))
        val m_axi_gmem0_ARBURST = Output(UInt(2.W))
        val m_axi_gmem0_ARLOCK = Output(UInt(2.W))
        val m_axi_gmem0_ARCACHE = Output(UInt(4.W))
        val m_axi_gmem0_ARPROT = Output(UInt(3.W))
        val m_axi_gmem0_ARQOS = Output(UInt(4.W))
        val m_axi_gmem0_ARREGION = Output(UInt(4.W))
        val m_axi_gmem0_ARUSER = Output(UInt(C_M_AXI_GMEM0_ARUSER_WIDTH.W))
        val m_axi_gmem0_RREADY = Output(Bool())
        val m_axi_gmem0_BREADY = Output(Bool())
        val s_axi_control_AWREADY = Output(Bool())
        val s_axi_control_WREADY = Output(Bool())
        val s_axi_control_ARREADY = Output(Bool())
        val s_axi_control_RVALID = Output(Bool())
        val s_axi_control_RDATA = Output(UInt(C_S_AXI_CONTROL_DATA_WIDTH.W))
        val s_axi_control_RRESP = Output(UInt(2.W))
        val s_axi_control_BVALID = Output(Bool())
        val s_axi_control_BRESP = Output(UInt(2.W))
        val interrupt = Output(Bool())
    })

    addPath(s"/scratch/schwarzem/firesim-cs267/target-design/chipyard/generators/vadd_soc/tl0_vadd_tl_vadd/src/main/verilog/tl0_vadd_tl_vadd_control_s_axi.v")
    addPath(s"/scratch/schwarzem/firesim-cs267/target-design/chipyard/generators/vadd_soc/tl0_vadd_tl_vadd/src/main/verilog/tl0_vadd_tl_vadd_gmem0_m_axi.v")
    addPath(s"/scratch/schwarzem/firesim-cs267/target-design/chipyard/generators/vadd_soc/tl0_vadd_tl_vadd/src/main/verilog/tl0_vadd_tl_vadd.v")

}


