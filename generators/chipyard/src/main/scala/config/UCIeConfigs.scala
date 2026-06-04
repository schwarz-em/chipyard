package chipyard

import org.chipsalliance.cde.config.{Config}
import freechips.rocketchip.diplomacy.{AddressSet}
import freechips.rocketchip.subsystem.{SBUS}
import testchipip.soc.{OBUS}
import edu.berkeley.cs.uciedigital.tilelink.{UcieTLParams}

class UcieChipletConfig extends Config(
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
          address = 0x8000,
          managerWhere = SBUS,
          numLanes = 16,
          includeDefaultModels = true
        )
      )
    )
  ) ++
  new chipyard.RocketConfig
)

class DualUcieConfig extends Config(
  new chipyard.harness.WithMultiChipD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieChipletConfig)
)