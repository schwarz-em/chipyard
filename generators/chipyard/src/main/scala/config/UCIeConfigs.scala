package chipyard

import org.chipsalliance.cde.config.{Config}
import freechips.rocketchip.diplomacy.{AddressSet}
import freechips.rocketchip.subsystem.{SBUS}
import testchipip.soc.{OBUS}
import edu.berkeley.cs.uciedigital.tilelink.{UcieTLParams}

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
// clock group of its own.
class UcieNoPhyChipletConfig extends Config(
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
