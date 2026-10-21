// See LICENSE for license details.

package firechip.bridgeinterfaces

import chisel3._

// A D2D link bridge carries one fixed-width token per target cycle in each
// direction between two simulated chips. It knows nothing about the link
// protocol: a target-side adapter (see firechip.bridgestubs.D2DLinkAdapter)
// packs everything the link drives (data, valid, ready, credits, ...) into the
// outgoing token, and unpacks the peer's token onto the link's inputs.

// The bridge module's constructor argument. Must be a case class so it can be
// serialized into the bridge annotation.
// How tokens are packed into host stream beats is up to the Golden Gate module.
case class D2DLinkBridgeParams(tokenBits: Int) {
  require(tokenBits > 0, "D2D link token must be at least one bit wide")
}

class D2DLinkBridgeTargetIO(val tokenBits: Int) extends Bundle {
  val clock = Input(Clock())
  val reset = Input(Bool())
  // This chip's token for the current target cycle, sent to the peer.
  val tx = Input(UInt(tokenBits.W))
  // The peer's token from `latency` target cycles ago.
  val rx = Output(UInt(tokenBits.W))
}
