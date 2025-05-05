// See LICENSE for license details

package firechip.bridgestubs

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.Parameters

import firesim.lib.bridgeutils._

import firechip.bridgeinterfaces._

// This is the id of the chip we want to connect to (for args passing)
case class CTCKey(otherChipId: Int)

class CTCBridge(otherChipId: Int)(implicit p: Parameters) extends BlackBox
    with Bridge[HostPortIO[CTCBridgeTargetIO]] {

  val moduleName = "firechip.goldengateimplementations.CTCBridgeModule"

  val io = IO(new CTCBridgeTargetIO)

  val bridgeIO = HostPort(io)

  val constructorArg = Some(CTCKey(otherChipId))

  generateAnnotations()
}

object CTCBridge {
  def apply(clock: Clock, port: testchipip.ctc.CTCBridgeIO, reset: Bool, otherChipId: Int)(implicit p: Parameters): CTCBridge = {
    val ep = Module(new CTCBridge(otherChipId))
    ep.io.ctc_io <> port
    ep.io.clock := clock
    ep.io.reset := reset
    ep
  }
}
