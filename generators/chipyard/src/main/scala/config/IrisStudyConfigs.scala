package chipyard

import scala.collection.immutable.ListMap

import org.chipsalliance.cde.config.{Config, Parameters}
import freechips.rocketchip.diplomacy.{AddressSet}
import freechips.rocketchip.subsystem.{SBUS}
import testchipip.soc.{ChipletRoutingParams, OffchipRouterParams}
import edu.berkeley.cs.uciedigital.tilelink.{UcieTLParams}
import saturn.common.{VectorParams}

// ============================================================================
// SRAM-vs-BW study chiplet (sram-vs-bw-study/plans/M2.md, W2)
//
// Iris (iris 0d5ab69 parameters) for FireSim, with the changes the study needs:
//   * 32-B SBUS (WithIrisBusTweaks; commented out in IrisConfig)
//   * main memory on an AXI4 ExtMem port (FASED + HBM model in FireSim)
//     instead of the serial-TL manager port, with an incoherent L2-bypass
//     alias for weight/KV streams (M1 decision D5)
//   * an SBUS scratchpad sized by the study (8 MB / 16 MB builds)
//   * two PHY-less UCIe ports on the chiplet router (M5), as UCIeFireSimConfig
//   * 1 GHz tiles, buses and d2d
//   * no Tacit trace
// Everything else (tiles, Saturn/OPU, TCMs, L2, NoC topology) is IrisConfig's.
// ============================================================================

object IrisStudy {
  val spadBase   = BigInt("580000000", 16)
  val dramBase   = BigInt("80000000", 16)       // rocket-chip ExtMem default base
  val dramBytes  = BigInt(1) << 30               // 1 GiB: the 42M blob is 47 MB
  // L2-bypass alias of ExtMem: the alias spans [bypassBase, 2 * bypassBase).
  // Not rocket-chip's default 0x10_0000_0000 (it overlaps
  // WithOffchipAddressRange(0x8_0000_0000, 0x20_0000_0000)), and not above the
  // off-chip range either: [0x40_0000_0000, 0x80_0000_0000) needs 39 paddr
  // bits, and Sv39 requires vaddrBits (39) > paddrBits. [8 GiB, 16 GiB) is free
  // (scratchpad 0x5_8000_0000, off-chip from 0x8_0000_0000) and keeps paddr at
  // 38 bits. DRAM 0x8000_0000 is reached uncached at 0x2_8000_0000.
  val bypassBase = BigInt("200000000", 16)
  val ucieBases  = Seq(BigInt(0x200000), BigInt(0x208000)) // as iris 0d5ab69

  // Substring that names the ExtMem path's SBUS edges in the NoC: the four L2
  // bank edges and the L2-bypass edge all carry the memory port's slave name,
  // told apart only by Constellation's "[k]|" suffix, so one key maps all five
  // to the L2 node. Elaboration names them "system[0]|".."system[3]|" (L2
  // banks) and "system[4]|" (bypass). iris-cy used "serdesser" when memory sat
  // behind the serial-TL manager port.
  val memEdgeKey = "system["
}

/** Main memory on AXI4 ExtMem plus the L2-bypass alias. Replaces the serial-TL
  * manager port (WithIrisStudyUncore drops WithNoMemPort and that port).
  */
class WithIrisStudyMemory(nChannels: Int = 1) extends Config(
  new freechips.rocketchip.subsystem.WithExtMemSbusBypass(IrisStudy.bypassBase) ++
  new freechips.rocketchip.subsystem.WithExtMemSize(IrisStudy.dramBytes) ++
  new freechips.rocketchip.subsystem.WithNMemoryChannels(nChannels)
)

/** Two PHY-less UCIe ports on the chiplet router, timed in FireSim by
  * WithD2DLinkBridge's UcieLinkModel (0.5 frames/cycle, 83 cycles one way at
  * 1 GHz). The off-chip address range is already set by WithIrisUncore.
  */
class WithIrisStudyUcie extends Config(
  new testchipip.soc.WithChipletRouting(ChipletRoutingParams(
    routerParams = OffchipRouterParams(tableEntries = 4),
    ports = IrisStudy.ucieBases.map { a =>
      UcieTLParams(address = a, managerWhere = SBUS, phy = false, noPhyTxQueueDepth = 32)
    }))
)

/** WithIrisUncore (iris-cy) minus WithNoMemPort and the serial-TL manager port,
  * with the scratchpad sized by the study. The serial-TL client port (TSI
  * loading, HTIF) comes from AbstractConfig: 32-bit, external clock.
  */
class WithIrisStudyUncore(spadBytes: BigInt) extends Config(
  new freechips.rocketchip.subsystem.WithoutTLMonitors ++
  new testchipip.soc.WithOffchipAddressRange(AddressSet.misaligned(0x800000000L, 0x2000000000L)) ++

  // L2, as Iris
  new chipyard.config.WithInclusiveCacheInteriorBuffer ++
  new chipyard.config.WithInclusiveCacheExteriorBuffer ++
  new freechips.rocketchip.subsystem.WithInclusiveCache(nWays = 4, capacityKB = 512, outerLatencyCycles = 4) ++
  new freechips.rocketchip.subsystem.WithNBanks(4) ++

  // SBUS scratchpad: Iris's 2 banks at 0x5_8000_0000, study-sized
  new testchipip.soc.WithNoScratchpadMonitors ++
  new testchipip.soc.WithScratchpad(
    base = IrisStudy.spadBase,
    size = spadBytes,
    banks = 2,
    partitions = 1
  ) ++

  new testchipip.soc.WithNoScratchpads ++   // drops AbstractConfig's MBUS scratchpad
  new chipyard.config.AbstractConfig
)

/** The study chiplet (no harness/FireSim specifics). */
class IrisStudyConfig(spadBytes: BigInt = BigInt(8) << 20, memChannels: Int = 1, sbusNoC: Boolean = true) extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++

  // 1 GHz everywhere (tiles, buses, d2d), as the UCIe bridge configs
  new chipyard.WithUcieChipletFrequency(1000.0) ++

  new WithIrisStudyUcie ++
  new WithIrisStudyMemory(memChannels) ++

  // ==================================
  // Buses: IrisConfig's SBUS NoC, plus the study's new edges
  // ==================================
  (if (!sbusNoC) Parameters.empty else new constellation.soc.WithSbusNoC(
    IrisNoCParams(
      constellation.protocol.DiplomaticNetworkNodeMapping(
        inNodeMapping = ListMap(
          "Core 0 ICache" -> 0,      // Shuttle 0 (left)
          "Core 1 ICache" -> 2,      // Shuttle 1 (right)
          "serial_tl_0_0[0]" -> 6,   // Front BUS
          "Core 2 DCache" -> 4,      // RocketTile (Saturn DMA)
          "stream[" -> 4,            // mempress memcopy read/write helpers on hart 2 (IrisStudyMempressRTLConfig only)
          "ucie-client" -> 5         // both UCIe client edges ("[0]|", "[1]|")
        ),
        outNodeMapping = ListMap(
          "Core 0 TCM" -> 0,         // Shuttle 0 TCM (left)
          "Core 1 TCM" -> 2,         // Shuttle 1 TCM (right)
          "ctrls[0]" -> 6,           // PBUS
          IrisStudy.memEdgeKey -> 3, // L2 banks 0-3 and the L2 bypass (MBUS side)
          "ucie[0]" -> 5,            // both UCIe ports: the chiplet router presents one manager edge
          "ram[0]|" -> 1,            // SBUS scratchpad bank 0
          "ram[1]|" -> 1             // SBUS scratchpad bank 1
        )
      )
    ),
    inlineNoC = true
  )) ++
  new WithIrisBusTweaks ++           // 32-B SBUS

  // ==================================
  // Rocket + Saturn DMA (hart 2)
  // ==================================
  new saturn.rocket.WithRocketVectorUnit(256, 256, VectorParams.dmaParams) ++
  new WithIrisRocketCore ++

  // ==================================
  // Shuttle tiles + Saturn/OPU (harts 0-1)
  // ==================================
  new saturn.shuttle.WithShuttleVectorUnit(256, 128, VectorParams.opuMxParams) ++
  new shuttle.common.WithShuttleTileBeatBytes(16) ++      // iris 0d5ab69 (iris-cy: 32)
  new shuttle.common.WithTCM(size = 128L << 10, banks = 2) ++
  new shuttle.common.WithShuttleTileBoundaryBuffers() ++
  new shuttle.common.WithL1ICacheWays(2) ++
  new shuttle.common.WithL1ICacheSets(64) ++
  new shuttle.common.WithL1DCacheWays(2) ++
  new shuttle.common.WithL1DCacheBanks(1) ++
  new shuttle.common.WithL1DCacheTagBanks(1) ++
  new shuttle.common.WithNShuttleCores(2) ++

  new WithIrisStudyUncore(spadBytes)
)

/** Chipyard RTL sim (sims/vcs) of the study chiplet: both D2D ports tied off,
  * memory on the harness's SimDRAM. For the L2-bypass, DMA -> TCM and
  * address-map checks before FireSim (plans/M2.md gate 1).
  */
class IrisStudyRTLConfig extends Config(
  new chipyard.harness.WithD2DTiedOff ++
  new IrisStudyConfig
)

/** IrisStudyRTLConfig plus the mempress memcopy engine (mempress `dma` branch,
  * RoCC custom2) on Rocket hart 2, next to Saturn's DMA unit, for the DMA
  * engine comparison (plans/M2.md). Separate read and write L2 helpers, 32-B
  * beats, 32 outstanding requests each. Shuttle tiles also read BuildRoCC, so
  * WithMultiRoCC scopes it to hart 2.
  */
class IrisStudyMempressRTLConfig extends Config(
  new chipyard.config.WithMultiRoCC ++
  new chipyard.config.WithMultiRoCCFromBuildRoCC(2) ++
  new mempress.WithMemPress(maxStreams = 1, beatBytes = 32, useMemLoader = true) ++
  new IrisStudyRTLConfig
)

/** IrisStudyMempressRTLConfig with a plain crossbar SBUS instead of Iris's
  * Constellation NoC: diagnostic, to tell whether the NoC caps DMA copies at
  * ~5 B/cycle (plans/M2.md).
  */
class IrisStudyXbarMempressRTLConfig extends Config(
  new chipyard.config.WithMultiRoCC ++
  new chipyard.config.WithMultiRoCCFromBuildRoCC(2) ++
  new mempress.WithMemPress(maxStreams = 1, beatBytes = 32, useMemLoader = true) ++
  new chipyard.harness.WithD2DTiedOff ++
  new IrisStudyConfig(sbusNoC = false)
)
