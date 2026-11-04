package chipyard

import org.chipsalliance.cde.config.{Config}
import freechips.rocketchip.diplomacy.{AddressSet}
import freechips.rocketchip.subsystem.{SBUS}
import testchipip.soc.{OBUS}
import edu.berkeley.cs.uciedigital.tilelink.{UcieTLParams, UcieLinkModelParams}
import saturn.common.{VectorParams}

class UcieChipletConfig extends Config(
  new chipyard.iobinders.WithD2DPunchthrough ++
  new chipyard.iobinders.WithSerialTLPunchthrough ++
  new chipyard.clocking.ClockNameContainsAssignment("d2d", 500.0) ++
  new chipyard.harness.WithUciePhyBypassClocks ++
  new testchipip.soc.WithOffchipAddressRange(
    AddressSet.misaligned(0x800000000L, 0x2000000000L)
  ) ++
  new testchipip.soc.WithChipletRouting(
    testchipip.soc.ChipletRoutingParams(
      routerParams =
        testchipip.soc.OffchipRouterParams(tableEntries = 4),
      ports = Seq(
        UcieTLParams(
          // Its clock registers sit 0x10000 above the base, so 0x8000 would
          // overlap the boot ROM.
          address = 0x200000,
          managerWhere = SBUS,
          numLanes = 16,
          maxInflight = 32,
          includeDefaultModels = true,
          sim = true,
          latencyMonitor = true
        )
      )
    )
  ) ++
  new chipyard.RocketConfig
)

class DualUcieConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieChipletConfig)
)

// UCIe's TileLink frames without the PHY: the D2D port is a `UciePacketIO`
// (one frame per cycle each way, credits in band), for simulators that model
// the link themselves, e.g. FireSim's D2D link bridge. No UCIe registers, so
// software does not call `setup_ucie()`. On its own the port is looped back.
// The link runs on the system bus clock (the chiplet router's), so it has no
// clock group of its own. `txQueueDepth` and `rxClockRatio` are
// `UcieTLParams.noPhyTxQueueDepth` and `noPhyRxClockRatio`.
class UcieNoPhyChipletConfig(txQueueDepth: Int = 0, rxClockRatio: Double = 0.0) extends Config(
  new chipyard.harness.WithD2DLoopback ++
  new chipyard.iobinders.WithD2DPunchthrough ++
  new chipyard.iobinders.WithSerialTLPunchthrough ++
  new testchipip.soc.WithOffchipAddressRange(
    AddressSet.misaligned(0x800000000L, 0x2000000000L)
  ) ++
  new testchipip.soc.WithChipletRouting(
    testchipip.soc.ChipletRoutingParams(
      routerParams =
        testchipip.soc.OffchipRouterParams(tableEntries = 4),
      ports = Seq(
        UcieTLParams(
          address = 0x8000,
          managerWhere = SBUS,
          phy = false,
          noPhyTxQueueDepth = txQueueDepth,
          noPhyRxClockRatio = rxClockRatio,
          latencyMonitor = true
        )
      )
    )
  ) ++
  new chipyard.RocketConfig
)

class DualUcieNoPhyConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieNoPhyChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieNoPhyChipletConfig)
)

// Runs a UCIe chiplet's link, buses and cores all at `freqMHz`. They have to
// move together: the chiplet router needs its buses at one frequency, and the
// link's TL and register ports reach them without a clock crossing. The cores
// follow so `rdcycle` counts bus cycles. The PHY's own clocks (the harness's
// bypass, sideband and reference clocks) are unaffected.
class WithUcieChipletFrequency(freqMHz: Double) extends Config(
  new chipyard.clocking.ClockNameContainsAssignment("d2d", freqMHz) ++
  new chipyard.config.WithTileFrequency(freqMHz) ++
  new chipyard.config.WithUniformBusFrequencies(freqMHz)
)

class DualUcie1GHzConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new WithUcieChipletFrequency(1000.0) ++ new UcieChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new WithUcieChipletFrequency(1000.0) ++ new UcieChipletConfig)
)

class DualUcieNoPhy1GHzConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig)
)

// `DualUcieNoPhy1GHzConfig` with the link timed like `DualUcie1GHzConfig`'s
// (controller mode, 16 GT/s): for checking `UcieLinkModel` against the RTL.
class DualUcieNoPhyModel1GHzConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieLinkModel(chip0=1, chip1=0, params=UcieLinkModelParams(fSbusMHz = 1000.0, linkLatency = 83)) ++
  new chipyard.harness.WithMultiChip(0, new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig)
)

// The 1 GHz pair with a Saturn DMA vector unit on each chip's core, for traffic
// with many requests in flight. The vector unit's memory port is as wide as
// the system bus (64 bits), so each request is one beat.
class WithUcieSaturnDMA extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 64, VectorParams.dmaParams)
)

class DualUcieSaturn1GHzConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieChipletConfig)
)

class DualUcieNoPhyModelSaturn1GHzConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieLinkModel(chip0=1, chip1=0, params=UcieLinkModelParams(fSbusMHz = 1000.0, linkLatency = 83)) ++
  new chipyard.harness.WithMultiChip(0, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig)
)

// `DualUcieNoPhyModelSaturn1GHzConfig` with a 32-frame queue in front of the
// link model, like the controller's `txAQ`.
class DualUcieNoPhyModelQueueSaturn1GHzConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieLinkModel(chip0=1, chip1=0, params=UcieLinkModelParams(fSbusMHz = 1000.0, linkLatency = 83)) ++
  new chipyard.harness.WithMultiChip(0, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig(txQueueDepth = 32)) ++
  new chipyard.harness.WithMultiChip(1, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig(txQueueDepth = 32))
)

// The queued model plus the controller's RX crossing timing (800 MHz sideband
// into the 1 GHz link clock).
class DualUcieNoPhyModelQueueRxSaturn1GHzConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieLinkModel(chip0=1, chip1=0, params=UcieLinkModelParams(fSbusMHz = 1000.0, linkLatency = 83)) ++
  new chipyard.harness.WithMultiChip(0, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig(txQueueDepth = 32, rxClockRatio = 0.8)) ++
  new chipyard.harness.WithMultiChip(1, new WithUcieSaturnDMA ++ new WithUcieChipletFrequency(1000.0) ++ new UcieNoPhyChipletConfig(txQueueDepth = 32, rxClockRatio = 0.8))
)
