package chipyard

import org.chipsalliance.cde.config.{Config}
import freechips.rocketchip.diplomacy.AddressSet
import freechips.rocketchip.subsystem.SBUS
import saturn.common.{VectorParams}

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
  new testchipip.soc.WithMaxOffchipAddressRange(
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
  new testchipip.soc.WithMaxOffchipAddressRange(
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
  new chipyard.harness.WithMultiChipD2D(chip0=1, chip1=0, chip0portId=0, chip1portId=0) ++ 
  new chipyard.harness.WithMultiChip(0, new SinglecoreIrisConfig) ++
  new chipyard.harness.WithMultiChip(1, new SinglecoreIrisConfig) 
)
