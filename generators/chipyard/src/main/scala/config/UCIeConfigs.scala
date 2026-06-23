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
          maxInflight = 32,
          includeDefaultModels = true
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

// Single-chip Rocket + UCIe with the dsp25 DMA engine attached on the SBUS
// (audio peripheral handshakes tied off; only the mem-to-mem CPU channels used).
class UcieDmaChipletConfig extends Config(
  new chipyard.iobinders.WithDMATilePeripheralTieoff ++
  new dsp25_audio.WithDMATile() ++
  new UcieChipletConfig
)

class UcieDmaDualConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieDmaChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieDmaChipletConfig)
)

// Single-chip Rocket + UCIe with the mempress accelerator in memcopy mode
// (MemLoader reads, MemWriter writes; beatBytes=32 matches the 256-bit beat
// the L2 helper presents through its TLWidthWidget).
class UcieMempressChipletConfig extends Config(
  new mempress.WithMemPress(maxStreams=1, beatBytes=32, useMemLoader=true) ++
  new UcieChipletConfig
)

class UcieMempressDualConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieMempressChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieMempressChipletConfig)
)