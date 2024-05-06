package hls_rocc0_vadd_vadd

import sys.process._

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config.{Parameters, Field}
import freechips.rocketchip.tile._
import freechips.rocketchip.util._
import vivadoHLS._

class rocc0_vadd_vadd() extends BlackBox() with HasBlackBoxPath {
    val io = IO(new Bundle {
        val ap_clk = Input(Clock())
        val ap_rst = Input(Bool())
        val ap_start = Input(Bool())
        val length_a_req_full_n = Input(Bool())
        val length_a_rsp_empty_n = Input(Bool())
        val length_a_datain = Input(UInt(32.W))
        val b_c_req_full_n = Input(Bool())
        val b_c_rsp_empty_n = Input(Bool())
        val b_c_datain = Input(UInt(32.W))
        val ap_done = Output(Bool())
        val ap_idle = Output(Bool())
        val ap_ready = Output(Bool())
        val length_a_req_din = Output(Bool())
        val length_a_req_write = Output(Bool())
        val length_a_rsp_read = Output(Bool())
        val length_a_address = Output(UInt(32.W))
        val length_a_dataout = Output(UInt(32.W))
        val length_a_size = Output(UInt(32.W))
        val b_c_req_din = Output(Bool())
        val b_c_req_write = Output(Bool())
        val b_c_rsp_read = Output(Bool())
        val b_c_address = Output(UInt(32.W))
        val b_c_dataout = Output(UInt(32.W))
        val b_c_size = Output(UInt(32.W))
        val ap_return = Output(UInt(32.W))
    })

	addPath(s"/scratch/schwarzem/firesim-cs267/target-design/chipyard/generators/vadd_soc/rocc0_vadd_vadd/src/main/verilog/rocc0_vadd_vadd.v")

}


class HLSrocc0_vadd_vaddBlackbox() extends Module {
	val scalar_io_dataWidths = List()
	val scalar_io_argLoc = List() //Lists the argument number of the scalar_io
	val ap_bus_addrWidths = List(64,64)
	val ap_bus_dataWidths = List(32,32)
	val ap_bus_argLoc = List(0,1)
	val io = IO(new Bundle {
	    val ap = new ApCtrlIO(dataWidth = 32)
	    val ap_bus = HeterogeneousBag.apply(ap_bus_addrWidths.zip(ap_bus_dataWidths).map {
                        case (aw, dw) => new ApBusIO(dw, aw)
                        })
        
    })

	val bb = Module(new rocc0_vadd_vadd())

	bb.io.ap_start := io.ap.start
	io.ap.done := bb.io.ap_done
	io.ap.idle := bb.io.ap_idle
	io.ap.ready := bb.io.ap_ready

    io.ap.rtn := bb.io.ap_return
    bb.io.ap_rst := reset
    bb.io.ap_clk := clock

    io.ap_bus(0).req.din := bb.io.length_a_req_din
    bb.io.length_a_req_full_n := io.ap_bus(0).req_full_n
    io.ap_bus(0).req_write := bb.io.length_a_req_write
    bb.io.length_a_rsp_empty_n := io.ap_bus(0).rsp_empty_n
    io.ap_bus(0).rsp_read := bb.io.length_a_rsp_read
    io.ap_bus(0).req.address := bb.io.length_a_address
    bb.io.length_a_datain := io.ap_bus(0).rsp.datain
    io.ap_bus(0).req.dataout := bb.io.length_a_dataout
    io.ap_bus(0).req.size := bb.io.length_a_size
    io.ap_bus(1).req.din := bb.io.b_c_req_din
    bb.io.b_c_req_full_n := io.ap_bus(1).req_full_n
    io.ap_bus(1).req_write := bb.io.b_c_req_write
    bb.io.b_c_rsp_empty_n := io.ap_bus(1).rsp_empty_n
    io.ap_bus(1).rsp_read := bb.io.b_c_rsp_read
    io.ap_bus(1).req.address := bb.io.b_c_address
    bb.io.b_c_datain := io.ap_bus(1).rsp.datain
    io.ap_bus(1).req.dataout := bb.io.b_c_dataout
    io.ap_bus(1).req.size := bb.io.b_c_size

}
