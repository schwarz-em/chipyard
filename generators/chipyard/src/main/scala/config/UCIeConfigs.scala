package chipyard

import org.chipsalliance.cde.config.{Config}
import freechips.rocketchip.diplomacy.{AddressSet}
import freechips.rocketchip.subsystem.{SBUS}
import testchipip.soc.{OBUS}
import edu.berkeley.cs.uciedigital.tilelink.{UcieTLParams}
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
  new chipyard.config.WithSystemBusWidth(256) ++
  new UcieChipletConfig
)

class UcieMempressDualConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieMempressChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieMempressChipletConfig)
)

// Single-chip Rocket + UCIe with a dedicated Saturn DMA core in place of mempress.
// UcieChipletConfig's WithNHugeCores(1) leaves tile 0 at id 0, so the second
// WithNHugeCores(1) below appends a new huge Rocket tile at id 1 (idOffset is
// additive, mirroring DMAV256D256RocketConfig); cores=Some(Seq(1)) restricts the
// Saturn vector unit build to that new tile only, so the original compute core
// (tile 0) keeps its default FPU/mulDiv setup instead of also being clobbered
// into a memcpy-only core. dLen=vLen=256 and VectorParams.dmaParams match
// DMAV256D256RocketConfig, stripping all arithmetic FUs from the DMA tile's
// vector pipeline so it only executes memcpys.
//
// WithSystemBusWidth(256) is required, not cosmetic: useL1DCache=true (the
// WithRocketVectorUnit default) sets the DMA tile's dcache rowBits = dLen =
// 256 bits, and rocket-chip's TLB/PMA logic (TLBPageLookup) then requires
// every manager reachable on the bus -- including the UCIe register file at
// 0x8000 -- to support 32-byte Gets. UcieChipletConfig's default 8-byte SBUS
// generates that register file narrower than that, which fails elaboration
// with "Memory region 'regs' ... only supports TransferSizes[1, 8] Get, but
// must support TransferSizes[1, 32]". Widening the SBUS (as
// DMAV256D256RocketConfig itself does) makes the register file get
// generated against the wider bus in the first place, matching what
// UcieSaturnDMASinglecoreIrisConfig gets for free from IrisConfig's own
// WithSystemBusWidth(256).
class UcieSaturnDMAChipletConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 256, VectorParams.dmaParams, cores = Some(Seq(1))) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new UcieChipletConfig
)

class UcieSaturnDMADualConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieSaturnDMAChipletConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieSaturnDMAChipletConfig)
)