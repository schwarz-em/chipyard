package hls_tl0_vadd_tl_vadd

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.{Parameters, Field, Config}
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.tilelink._
import freechips.rocketchip.amba.axi4._
import freechips.rocketchip.util._ 
import freechips.rocketchip.prci._
import freechips.rocketchip.subsystem._

//case class tl0_vadd_tl_vaddConfig()
//case object tl0_vadd_tl_vaddKey extends Field[Option[tl0_vadd_tl_vaddConfig]](None)

class HLStl0_vadd_tl_vaddAXI (address: BigInt = 0x20000, beatBytes: Int = 8) (implicit p: Parameters) extends ClockSinkDomain(ClockSinkParameters())(p) {  

    val numInFlight = 8

    
    val node_gmem0 = AXI4MasterNode(Seq(AXI4MasterPortParameters(
        masters = Seq(AXI4MasterParameters(
            name = "axil_hub_mem_out_0",
            id = IdRange(0, numInFlight),
            aligned = true,
        maxFlight = Some(8)
      ))
      )
    ))
 
    val slave_node = AXI4SlaveNode(Seq(AXI4SlavePortParameters(
        slaves = Seq(AXI4SlaveParameters(
            address = List(AddressSet(address,0x4000-1)),
            regionType = RegionType.UNCACHED,
            supportsWrite = TransferSizes(1, beatBytes),
            supportsRead = TransferSizes(1, beatBytes),
            interleavedId = Some(0)
        )),
        beatBytes = beatBytes
    )))

    override lazy val module = new HLStl0_vadd_tl_vaddAXIModule
    class HLStl0_vadd_tl_vaddAXIModule extends Impl {  
        withClockAndReset(clock, reset) {
            //val (out, edge) = outer.node.out(0)
            val (slave_in, slave_edge) = slave_node.in(0)

            val bId = Reg(UInt(32.W))
            val rId = Reg(UInt(32.W))

            val bb = Module(new tl0_vadd_tl_vadd())

            val (out_gmem0, edge_gmem0) = node_gmem0.out(0)
    bb.io.ap_clk := clock
    bb.io.ap_rst_n := !reset.asBool

    bb.io.m_axi_gmem0_AWREADY := out_gmem0.aw.ready
    bb.io.m_axi_gmem0_WREADY := out_gmem0.w.ready
    bb.io.m_axi_gmem0_ARREADY := out_gmem0.ar.ready
    bb.io.m_axi_gmem0_RVALID := out_gmem0.r.valid
    bb.io.m_axi_gmem0_RDATA := out_gmem0.r.bits.data
    bb.io.m_axi_gmem0_RLAST := out_gmem0.r.bits.last
    bb.io.m_axi_gmem0_RID := out_gmem0.r.bits.id
    bb.io.m_axi_gmem0_RUSER := 0.U
    bb.io.m_axi_gmem0_RRESP := out_gmem0.r.bits.resp
    bb.io.m_axi_gmem0_BVALID := out_gmem0.b.valid
    bb.io.m_axi_gmem0_BRESP := out_gmem0.b.bits.resp
    bb.io.m_axi_gmem0_BID := out_gmem0.b.bits.id
    bb.io.m_axi_gmem0_BUSER := 0.U
    bb.io.s_axi_control_AWVALID := slave_in.aw.valid
    bb.io.s_axi_control_AWADDR := slave_in.aw.bits.addr
    bb.io.s_axi_control_WVALID := slave_in.w.valid
    bb.io.s_axi_control_WDATA := slave_in.w.bits.data
    bb.io.s_axi_control_WSTRB := slave_in.w.bits.strb
    bb.io.s_axi_control_ARVALID := slave_in.ar.valid
    bb.io.s_axi_control_ARADDR := slave_in.ar.bits.addr
    bb.io.s_axi_control_RREADY := slave_in.r.ready
    bb.io.s_axi_control_BREADY := slave_in.b.ready
    out_gmem0.aw.valid := bb.io.m_axi_gmem0_AWVALID
    out_gmem0.aw.bits.addr := bb.io.m_axi_gmem0_AWADDR
    out_gmem0.aw.bits.id := bb.io.m_axi_gmem0_AWID
    out_gmem0.aw.bits.len := bb.io.m_axi_gmem0_AWLEN
    out_gmem0.aw.bits.size := bb.io.m_axi_gmem0_AWSIZE
    out_gmem0.aw.bits.burst := bb.io.m_axi_gmem0_AWBURST
    out_gmem0.aw.bits.lock := bb.io.m_axi_gmem0_AWLOCK
    out_gmem0.aw.bits.cache := bb.io.m_axi_gmem0_AWCACHE
    out_gmem0.aw.bits.prot := bb.io.m_axi_gmem0_AWPROT
    out_gmem0.aw.bits.qos := bb.io.m_axi_gmem0_AWQOS
    //out_gmem0.aw.bits.region := bb.io.m_axi_gmem0_AWREGION
    
    out_gmem0.w.valid := bb.io.m_axi_gmem0_WVALID
    out_gmem0.w.bits.data := bb.io.m_axi_gmem0_WDATA
    out_gmem0.w.bits.strb := bb.io.m_axi_gmem0_WSTRB
    out_gmem0.w.bits.last := bb.io.m_axi_gmem0_WLAST
    
    
    out_gmem0.ar.valid := bb.io.m_axi_gmem0_ARVALID
    out_gmem0.ar.bits.addr := bb.io.m_axi_gmem0_ARADDR
    out_gmem0.ar.bits.id := bb.io.m_axi_gmem0_ARID
    out_gmem0.ar.bits.len := bb.io.m_axi_gmem0_ARLEN
    out_gmem0.ar.bits.size := bb.io.m_axi_gmem0_ARSIZE
    out_gmem0.ar.bits.burst := bb.io.m_axi_gmem0_ARBURST
    out_gmem0.ar.bits.lock := bb.io.m_axi_gmem0_ARLOCK
    out_gmem0.ar.bits.cache := bb.io.m_axi_gmem0_ARCACHE
    out_gmem0.ar.bits.prot := bb.io.m_axi_gmem0_ARPROT
    out_gmem0.ar.bits.qos := bb.io.m_axi_gmem0_ARQOS
    //out_gmem0.ar.bits.region := bb.io.m_axi_gmem0_ARREGION
    
    out_gmem0.r.ready :=bb.io.m_axi_gmem0_RREADY
    out_gmem0.b.ready :=bb.io.m_axi_gmem0_BREADY
    slave_in.aw.ready := bb.io.s_axi_control_AWREADY
    slave_in.w.ready := bb.io.s_axi_control_WREADY
    slave_in.ar.ready := bb.io.s_axi_control_ARREADY
    slave_in.r.valid := bb.io.s_axi_control_RVALID
    slave_in.r.bits.data := bb.io.s_axi_control_RDATA
    slave_in.r.bits.resp := bb.io.s_axi_control_RRESP
    slave_in.b.valid := bb.io.s_axi_control_BVALID
    slave_in.b.bits.resp := bb.io.s_axi_control_BRESP

            // For AXI4lite, these two signals are always True
            slave_in.r.bits.last := true.B 

            when(slave_in.aw.fire()){
                bId := slave_in.aw.bits.id
            }

            when(slave_in.ar.fire()){
                rId := slave_in.ar.bits.id 
            }
            slave_in.r.bits.id := rId
            slave_in.b.bits.id := bId
        }
    }
}

trait CanHavePeripheryHLStl0_vadd_tl_vaddAXI { this: BaseSubsystem =>
    //implicit val p: Parameters

    private val address = BigInt(0x30000)
    private val axi_m_portName = "HLS-Accelerator-tl0_vadd_tl_vadd-master"
    private val axilite_s_portName = "HLS-Accelerator-tl0_vadd_tl_vadd-slave"

    private val sbus = locateTLBusWrapper(SBUS)

    // Disable Config 
    //p(tl0_vadd_tl_vaddKey).map { k =>
      
        //val accel_s_axi_width = 32 
        //val hls_tl0_vadd_tl_vadd_accel = LazyModule(new HLStl0_vadd_tl_vaddAXI(address, sbus.beatBytes))
        val hls_tl0_vadd_tl_vadd_accel = LazyModule(new HLStl0_vadd_tl_vaddAXI(address, 32 >> 3))

        hls_tl0_vadd_tl_vadd_accel.clockNode := sbus.fixedClockNode
      
        
    sbus.coupleFrom(s"port_named_$axi_m_portName") {
      ( _
        := TLBuffer(BufferParams.default)
        := TLFIFOFixer(TLFIFOFixer.all)
        := TLWidthWidget(32 >> 3)
        := AXI4ToTL()
        := AXI4UserYanker(Some(8))
        := AXI4Fragmenter()
        := AXI4IdIndexer(1)
        := hls_tl0_vadd_tl_vadd_accel.node_gmem0
      )
    }


	//	mmioPortParamsOpt.map { params =>
		sbus.coupleTo(s"port_named_$axilite_s_portName") {
		  (hls_tl0_vadd_tl_vadd_accel.slave_node
			:= AXI4Buffer()
			:= AXI4UserYanker()
			:= AXI4Deinterleaver(sbus.blockBytes)
			:= AXI4IdIndexer(1)
			:= TLToAXI4()
            := TLFragmenter(32 >> 3, 64, alwaysMin=true, holdFirstDeny=true)
			:= TLWidthWidget(sbus.beatBytes)
			:= _)
		}
    //}
}

trait CanHavePeripheryHLStl0_vadd_tl_vaddAXIImp extends LazyModuleImp {
    val outer: CanHavePeripheryHLStl0_vadd_tl_vaddAXI
}

// class WithHLStl0_vadd_tl_vadd() extends Config((site, here, up) => {
//   case tl0_vadd_tl_vaddKey => Some(tl0_vadd_tl_vaddConfig())
// })
