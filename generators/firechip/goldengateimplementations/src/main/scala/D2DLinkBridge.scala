// See LICENSE for license details

package firechip.goldengateimplementations

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.Parameters

import midas.widgets._
import firesim.lib.bridgeutils._

import firechip.bridgeinterfaces._

/** Packs `tokensPerBeat` tokens into each stream beat, token 0 in the LSBs.
  * Accepts one token per cycle, including the cycle a full beat drains.
  */
class D2DTokenPacker(tokenBits: Int, tokensPerBeat: Int) extends Module {
  val io = IO(new Bundle {
    val token = Flipped(Decoupled(UInt(tokenBits.W)))
    val beat  = Decoupled(UInt(BridgeStreamConstants.streamWidthBits.W))
  })

  val buf   = Reg(Vec(tokensPerBeat, UInt(tokenBits.W)))
  val count = RegInit(0.U(log2Ceil(tokensPerBeat + 1).W))
  val full  = count === tokensPerBeat.U

  io.beat.valid := full
  io.beat.bits  := buf.asUInt // zero-extended to streamWidthBits
  io.token.ready := !full || io.beat.ready

  val idx = Mux(full, 0.U, count)
  when (io.token.fire) { buf(idx) := io.token.bits }
  count := MuxCase(count, Seq(
    (io.beat.fire && io.token.fire) -> 1.U,
    io.beat.fire                    -> 0.U,
    io.token.fire                   -> (count + 1.U)))
}

/** Splits each stream beat back into `tokensPerBeat` tokens, in packing order. */
class D2DTokenUnpacker(tokenBits: Int, tokensPerBeat: Int) extends Module {
  val io = IO(new Bundle {
    val beat  = Flipped(Decoupled(UInt(BridgeStreamConstants.streamWidthBits.W)))
    val token = Decoupled(UInt(tokenBits.W))
  })

  val tokens = io.beat.bits(tokensPerBeat * tokenBits - 1, 0).asTypeOf(Vec(tokensPerBeat, UInt(tokenBits.W)))
  val idx    = RegInit(0.U(log2Ceil(tokensPerBeat).max(1).W))
  val last   = idx === (tokensPerBeat - 1).U

  io.token.valid := io.beat.valid
  io.token.bits  := (if (tokensPerBeat == 1) tokens(0) else tokens(idx))
  io.beat.ready  := io.token.ready && last

  when (io.token.fire) { idx := Mux(last, 0.U, idx + 1.U) }
}

/** Protocol-agnostic D2D link bridge: one `tokenBits` token per target cycle in
  * each direction. The host driver (d2d_link_t) exchanges tokens with the peer
  * chip's driver and pre-loads `latency` cycles of empty tokens, which is what
  * models the link latency.
  */
class D2DLinkBridgeModule(key: D2DLinkBridgeParams)(implicit p: Parameters)
    extends BridgeModule[HostPortIO[D2DLinkBridgeTargetIO]]()(p)
    with StreamToHostCPU
    with StreamFromHostCPU {
  // Same depths as the CTC bridge (taken from the NIC). The from-host depth
  // bounds the link latency the driver can pre-load: latency / tokensPerBeat
  // beats must fit.
  val fromHostCPUQueueDepth = 3072
  val toHostCPUQueueDepth   = 3072

  lazy val module = new BridgeModuleImp(this) {
    val io    = IO(new WidgetIO)
    val hPort = IO(HostPort(new D2DLinkBridgeTargetIO(key.tokenBits)))

    val tokenBits = key.tokenBits
    require(tokenBits <= BridgeStreamConstants.streamWidthBits,
      s"D2D link token ($tokenBits bits) must fit in one ${BridgeStreamConstants.streamWidthBits}-bit stream beat")
    // Whole tokens per stream beat; the remainder of each beat is padding.
    val tokensPerBeat = BridgeStreamConstants.streamWidthBits / tokenBits

    // Target -> host: one token per target cycle (toHost fire).
    val packer = Module(new D2DTokenPacker(tokenBits, tokensPerBeat))
    packer.io.token.valid := hPort.toHost.hValid
    packer.io.token.bits  := hPort.hBits.tx
    hPort.toHost.hReady   := packer.io.token.ready
    streamEnq <> packer.io.beat

    // Host -> target: one token per target cycle (fromHost fire).
    val unpacker = Module(new D2DTokenUnpacker(tokenBits, tokensPerBeat))
    unpacker.io.beat <> streamDeq
    hPort.fromHost.hValid  := unpacker.io.token.valid
    hPort.hBits.rx         := unpacker.io.token.bits
    unpacker.io.token.ready := hPort.fromHost.hReady

    // Placeholder MMIO register: Golden Gate expects at least one, and the
    // driver's D2DLINKBRIDGEMODULE_struct must match.
    val tFire = hPort.toHost.hValid && hPort.fromHost.hReady
    genROReg(!tFire, "done")

    genCRFile()

    override def genHeader(base: BigInt, memoryRegions: Map[String, BigInt], sb: StringBuilder): Unit = {
      genConstructor(
        base,
        sb,
        "d2d_link_t",
        "d2d_link",
        Seq(
          UInt32(toHostStreamIdx),
          UInt32(toHostCPUQueueDepth),
          UInt32(fromHostStreamIdx),
          UInt32(fromHostCPUQueueDepth),
          UInt32(tokenBits),
          UInt32(tokensPerBeat),
        ),
        hasStreams = true,
      )
    }
  }
}
