// See LICENSE for license details

package firechip.bridgestubs

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.Parameters

import testchipip.soc.{ChipletIO}
import testchipip.ctc.{CTC, CTCBridgeIO}

import firesim.lib.bridgeutils._

import firechip.bridgeinterfaces._

class D2DLinkBridge(tokenBits: Int)(implicit p: Parameters) extends BlackBox
    with Bridge[HostPortIO[D2DLinkBridgeTargetIO]] {
  val moduleName = "firechip.goldengateimplementations.D2DLinkBridgeModule"
  val io = IO(new D2DLinkBridgeTargetIO(tokenBits))
  val bridgeIO = HostPort(io)
  val constructorArg = Some(D2DLinkBridgeParams(tokenBits))
  generateAnnotations()
}

/** Maps one kind of D2D link IO (a `ChipletIO`) onto the bridge's per-cycle
  * token.
  *
  * The bridge delivers the peer's `tx` token to this chip's `rx` a fixed number
  * of target cycles later, with nothing in between. The adapter therefore has to
  * make the link tolerate that latency: a valid-only link with in-band credits
  * (UCIe's packetized TileLink) maps over directly, while a ready/valid link
  * (CTC) only works if the receiver never drops a beat (see `CTCLinkAdapter`).
  */
trait D2DLinkAdapter[T <: ChipletIO] {
  def tokenBits: Int
  /** This chip's outgoing token, built from the link's outputs. */
  def tx(io: T): UInt
  /** Drive the link's inputs from the peer's token. */
  def rx(io: T, token: UInt): Unit
  /** The clock the link IO is synchronous to, if the IO carries it. Links
    * without one are assumed to run on the harness binder clock.
    */
  def clock(io: T): Option[Clock] = None
}

object D2DLinkBridge {
  def apply[T <: ChipletIO](io: T, adapter: D2DLinkAdapter[T], clock: Clock, reset: Bool)
      (implicit p: Parameters): D2DLinkBridge = {
    val ep = Module(new D2DLinkBridge(adapter.tokenBits))
    ep.io.clock := adapter.clock(io).getOrElse(clock)
    ep.io.reset := reset
    ep.io.tx := adapter.tx(io)
    adapter.rx(io, ep.io.rx)
    ep
  }
}

// ---------------------------------------------------------------------------
// CTC (no PHY): two flit channels, each a pair of Decoupled flit streams.
// ---------------------------------------------------------------------------

class CTCLinkToken extends Bundle {
  val client_data   = UInt(CTC.INNER_WIDTH.W)
  val client_valid  = Bool()
  val client_ready  = Bool()
  val manager_data  = UInt(CTC.INNER_WIDTH.W)
  val manager_valid = Bool()
  val manager_ready = Bool()
}

/** Same field mapping as the original CTCBridgeModule: my client channel talks
  * to the peer's manager channel and vice versa, and each side's `ready` is
  * forwarded as data.
  *
  * Caveat (inherited from CTCBridgeModule): with link latency L, my `out.ready`
  * is the peer's `in.ready` from L cycles ago, and the peer sees my `valid` L
  * cycles later. A beat is only delivered correctly if the receiving `in.ready`
  * is still high when it arrives. TODO: verify the CTC receivers are always
  * ready, or add credits/skid buffering here.
  */
object CTCLinkAdapter extends D2DLinkAdapter[CTCBridgeIO] {
  val tokenBits = (new CTCLinkToken).getWidth

  def tx(io: CTCBridgeIO): UInt = {
    val t = Wire(new CTCLinkToken)
    t.client_data   := io.client_flit.out.bits.flit
    t.client_valid  := io.client_flit.out.valid
    t.client_ready  := io.client_flit.in.ready
    t.manager_data  := io.manager_flit.out.bits.flit
    t.manager_valid := io.manager_flit.out.valid
    t.manager_ready := io.manager_flit.in.ready
    t.asUInt
  }

  def rx(io: CTCBridgeIO, token: UInt): Unit = {
    val peer = token.asTypeOf(new CTCLinkToken)
    // The peer's manager channel drives my client channel...
    io.client_flit.in.valid     := peer.manager_valid
    io.client_flit.in.bits.flit := peer.manager_data
    io.client_flit.out.ready    := peer.manager_ready
    // ...and the peer's client channel drives my manager channel.
    io.manager_flit.in.valid     := peer.client_valid
    io.manager_flit.in.bits.flit := peer.client_data
    io.manager_flit.out.ready    := peer.client_ready
  }
}

// ---------------------------------------------------------------------------
// UCIe (planned): the PHY-less UciePacketIO exposes UCIe's packetized,
// credit-based TileLink stream, so the adapter is a direct map:
//
//   class UcieLinkToken(packetBits: Int) extends Bundle {
//     val valid  = Bool()
//     val packet = UInt(packetBits.W)   // ~386 bits -> one token per beat
//   }
//   object UcieLinkAdapter extends D2DLinkAdapter[UciePacketIO] {
//     def tx(io) = Cat(io.tx.bits, io.tx.valid)
//     def rx(io, t) = { io.rx.valid := t(0); io.rx.bits := t >> 1 }
//     override def clock(io) = Some(io.clock)   // D2D clock domain
//   }
//
// No credits are needed in the adapter because UCIe returns credits in band;
// the credit pool (tlBufferDepth) just has to cover the round-trip latency.
// ---------------------------------------------------------------------------
