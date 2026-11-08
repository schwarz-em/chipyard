package chipyard.clocking

import org.chipsalliance.cde.config.{Config}
import freechips.rocketchip.util.{ClockGate, ClockGateImpl}

/** A ClockGate that never gates: out = in. For FireSim targets, whose FAME-1
  * transform cannot handle black-box clock gates (see WithoutClockGating in
  * firechip). Without this, a ClockGate inside a generator (e.g. Saturn's OPU
  * `opu_clock_gate`) becomes a LUT-gated fabric clock on the FPGA and breaks
  * hold timing.
  */
class PassthroughClockGate extends ClockGate {
  addResource("/vsrc/PassthroughClockGate.v")
}

/** Replace every rocket-chip ClockGate(...) instance with PassthroughClockGate. */
class WithPassthroughClockGates extends Config((site, here, up) => {
  case ClockGateImpl => () => new PassthroughClockGate
})
