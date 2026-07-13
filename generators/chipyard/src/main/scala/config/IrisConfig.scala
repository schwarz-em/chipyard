package chipyard

import org.chipsalliance.cde.config.{Config}
import freechips.rocketchip.diplomacy.AddressSet
import freechips.rocketchip.subsystem.SBUS
import saturn.common.{VectorParams}
import edu.berkeley.cs.uciedigital.tilelink.{UcieTLParams}

// Chipyard port of the iris config: two Saturn/OPU Shuttle cores with TCM,
// plus a chiplet router exposing two chip-to-chip serial-TL ports.
// All other settings inherit the chipyard AbstractConfig defaults (xbar bus
// topology rather than a NoC, plus the AbstractConfig TSI bringup serial-TL).

class IrisConfig extends Config(
  //-------------------------------------------------------------------
  // Two Shuttle cores with the iris Saturn vector + OPU + TCM setup
  //-------------------------------------------------------------------
  new saturn.shuttle.WithShuttleVectorUnit(512, 256, VectorParams.opuParams) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new shuttle.common.WithShuttleTileBeatBytes(32) ++
  new shuttle.common.WithTCM(size = 128L << 10, banks = 2) ++
  new shuttle.common.WithShuttleTileBoundaryBuffers() ++
  new shuttle.common.WithL1ICacheWays(2) ++
  new shuttle.common.WithL1ICacheSets(64) ++
  new shuttle.common.WithL1DCacheWays(2) ++
  new shuttle.common.WithL1DCacheBanks(1) ++
  new shuttle.common.WithL1DCacheTagBanks(1) ++
  new shuttle.common.WithNShuttleCores(2) ++

  //-------------------------------------------------------------------
  // Chip-to-chip serial-TL ports via a chiplet router (mirrors iris).
  //-------------------------------------------------------------------
  new chipyard.harness.WithD2DTiedOff ++
  new chipyard.iobinders.WithD2DPunchthrough ++
  new testchipip.soc.WithOffchipAddressRange(
    AddressSet.misaligned(0x800000000L, 0x2000000000L)) ++
  new testchipip.soc.WithChipletRouting(testchipip.soc.ChipletRoutingParams(
    routerParams = testchipip.soc.OffchipRouterParams(tableEntries = 4),
    ports = Seq(
      testchipip.serdes.SerialTLParams(
        client = Some(testchipip.serdes.SerialTLClientParams(masterWhere = SBUS)),
        manager = Some(testchipip.serdes.SerialTLManagerParams()),
        phyParams = testchipip.serdes.CreditedSourceSyncSerialPhyParams(),
        bundleParams = testchipip.serdes.TLSerdesser.STANDARD_TLBUNDLE_PARAMS.copy(dataBits = 256)
      ),
      testchipip.serdes.SerialTLParams(
        client = Some(testchipip.serdes.SerialTLClientParams(masterWhere = SBUS)),
        manager = Some(testchipip.serdes.SerialTLManagerParams()),
        phyParams = testchipip.serdes.CreditedSourceSyncSerialPhyParams(),
        bundleParams = testchipip.serdes.TLSerdesser.STANDARD_TLBUNDLE_PARAMS.copy(dataBits = 256)
      )
    ))) ++

  new chipyard.config.AbstractConfig)

class SinglecoreIrisConfig extends Config(
  //-------------------------------------------------------------------
  // Two Shuttle cores with the iris Saturn vector + OPU + TCM setup
  //-------------------------------------------------------------------
  new saturn.shuttle.WithShuttleVectorUnit(512, 256, VectorParams.opuParams) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new shuttle.common.WithShuttleTileBeatBytes(32) ++
  new shuttle.common.WithTCM(size = 128L << 10, banks = 2) ++
  new shuttle.common.WithShuttleTileBoundaryBuffers() ++
  new shuttle.common.WithL1ICacheWays(2) ++
  new shuttle.common.WithL1ICacheSets(64) ++
  new shuttle.common.WithL1DCacheWays(2) ++
  new shuttle.common.WithL1DCacheBanks(1) ++
  new shuttle.common.WithL1DCacheTagBanks(1) ++
  new shuttle.common.WithNShuttleCores(1) ++

  //-------------------------------------------------------------------
  // Chip-to-chip serial-TL ports via a chiplet router (mirrors iris).
  //-------------------------------------------------------------------
  new chipyard.harness.WithD2DTiedOff ++
  new chipyard.iobinders.WithD2DPunchthrough ++
  new testchipip.soc.WithOffchipAddressRange(
    AddressSet.misaligned(0x800000000L, 0x2000000000L)) ++
  new testchipip.soc.WithChipletRouting(testchipip.soc.ChipletRoutingParams(
    routerParams = testchipip.soc.OffchipRouterParams(tableEntries = 4),
    ports = Seq(
      testchipip.serdes.SerialTLParams(
        client = Some(testchipip.serdes.SerialTLClientParams(masterWhere = SBUS)),
        manager = Some(testchipip.serdes.SerialTLManagerParams()),
        phyParams = testchipip.serdes.CreditedSourceSyncSerialPhyParams(),
        bundleParams = testchipip.serdes.TLSerdesser.STANDARD_TLBUNDLE_PARAMS.copy(dataBits = 256)
      ),
      testchipip.serdes.SerialTLParams(
        client = Some(testchipip.serdes.SerialTLClientParams(masterWhere = SBUS)),
        manager = Some(testchipip.serdes.SerialTLManagerParams()),
        phyParams = testchipip.serdes.CreditedSourceSyncSerialPhyParams(),
        bundleParams = testchipip.serdes.TLSerdesser.STANDARD_TLBUNDLE_PARAMS.copy(dataBits = 256)
      )
    ))) ++

  new chipyard.config.AbstractConfig)

class DualIrisConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new SinglecoreIrisConfig) ++
  new chipyard.harness.WithMultiChip(1, new SinglecoreIrisConfig)
)

class UcieSinglecoreIrisConfig extends Config(
  //-------------------------------------------------------------------
  // Two Shuttle cores with the iris Saturn vector + OPU + TCM setup
  //-------------------------------------------------------------------
  new saturn.shuttle.WithShuttleVectorUnit(512, 256, VectorParams.opuParams) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new shuttle.common.WithShuttleTileBeatBytes(32) ++
  new shuttle.common.WithTCM(size = 128L << 10, banks = 2) ++
  new shuttle.common.WithShuttleTileBoundaryBuffers() ++
  new shuttle.common.WithL1ICacheWays(2) ++
  new shuttle.common.WithL1ICacheSets(64) ++
  new shuttle.common.WithL1DCacheWays(2) ++
  new shuttle.common.WithL1DCacheBanks(1) ++
  new shuttle.common.WithL1DCacheTagBanks(1) ++
  new shuttle.common.WithNShuttleCores(1) ++

  //-------------------------------------------------------------------
  // Chip-to-chip via a UCIe chiplet link (mirrors UcieChipletConfig).
  //-------------------------------------------------------------------
  new chipyard.clocking.ClockNameContainsAssignment("d2d", 500.0) ++
  new chipyard.harness.WithUciePhyBypassClocks ++
  new testchipip.soc.WithOffchipAddressRange(
    AddressSet.misaligned(0x800000000L, 0x2000000000L)) ++
  new testchipip.soc.WithChipletRouting(testchipip.soc.ChipletRoutingParams(
    routerParams = testchipip.soc.OffchipRouterParams(tableEntries = 4),
    ports = Seq(
      UcieTLParams(
        address = 0x8000,
        managerWhere = SBUS,
        numLanes = 16,
        maxInflight = 32,
        includeDefaultModels = true
      )
    ))) ++

  new chipyard.config.AbstractConfig)

class UcieDualIrisConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieSinglecoreIrisConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieSinglecoreIrisConfig)
)

// Single-chip Iris + UCIe with the dsp25 DMA engine attached on the SBUS
// (audio peripheral handshakes tied off; only the mem-to-mem CPU channels are used).
class UcieDmaSinglecoreIrisConfig extends Config(
  new chipyard.iobinders.WithDMATilePeripheralTieoff ++
  new dsp25_audio.WithDMATile() ++
  new UcieSinglecoreIrisConfig)

class UcieDmaDualIrisConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieDmaSinglecoreIrisConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieDmaSinglecoreIrisConfig)
)

// Single-chip Iris + UCIe with the mempress accelerator in memcopy mode
// (MemLoader reads, MemWriter writes; beatBytes=32 matches the 256-bit beat,
// MemPressMaxOutstandingReqs=32 matches the UCIe link maxInflight).
class UcieMempressSinglecoreIrisConfig extends Config(
  new mempress.WithMemPress(maxStreams=1, beatBytes=32, useMemLoader=true) ++
  new UcieSinglecoreIrisConfig)

class UcieMempressDualIrisConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieMempressSinglecoreIrisConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieMempressSinglecoreIrisConfig)
)

// Single-chip Iris + UCIe with a dedicated Saturn DMA core in place of mempress:
// a huge Rocket tile is appended (WithNHugeCores is additive, so the shuttle
// core is untouched) and given a Saturn vector unit built with VectorParams.dmaParams,
// which strips all arithmetic FUs so the vector pipeline only does memcpys.
// dLen=vLen=256 matches the 256-bit SBUS/UCIe beat, mirroring DMAV256D256RocketConfig.
class UcieSaturnDMASinglecoreIrisConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 256, VectorParams.dmaParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new UcieSinglecoreIrisConfig)

class UcieSaturnDMADualIrisConfig extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++
  new chipyard.harness.WithANDSuccessFn ++
  new chipyard.harness.WithMultiChipUcieD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++
  new chipyard.harness.WithMultiChip(0, new UcieSaturnDMASinglecoreIrisConfig) ++
  new chipyard.harness.WithMultiChip(1, new UcieSaturnDMASinglecoreIrisConfig)
)
