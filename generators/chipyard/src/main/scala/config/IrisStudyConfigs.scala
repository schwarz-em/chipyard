package chipyard

import scala.collection.immutable.ListMap

import org.chipsalliance.cde.config.{Config, Field, Parameters}
import freechips.rocketchip.diplomacy.{AddressSet}
import org.chipsalliance.diplomacy.lazymodule.{LazyModule}
import freechips.rocketchip.subsystem.{SBUS}
import testchipip.soc.{SubsystemInjector, SubsystemInjectorKey}
import freechips.rocketchip.tilelink.{TLRAM, TLFragmenter, TLBuffer, TLXbar}
import freechips.rocketchip.resources.{MemoryDevice}
import freechips.rocketchip.prci.{ClockSinkDomain, ClockSinkParameters}
import freechips.rocketchip.util.{ECCParams}
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
//   * no Tacit
//   * a crossbar SBUS instead of Iris's Constellation NoC (sbusNoC = false):
//     the NoC caps DMA copies at ~3.5-5.5 B/cycle (iris docs/NOC_BANDWIDTH.md)
//   * 32-B Shuttle tile port (iris 0d5ab69: 16 B), so a TCM fills at up to
//     32 B/cycle instead of ~15
//   * the mempress memcopy engine (RoCC, on the Rocket tile) for weight/KV
//     streams; Saturn's DMA unit on the Rocket tile is removed (rocketVector)
//   * one Shuttle tile with an int8-only OPU (VectorParams.opuParams), not two
//     with the MX/FP8 OPU: two MX Shuttles (655K LUTs of vector unit each) didn't
//     fit an F2 VU47P (user decision 2026-11-06). Rocket is then hart nShuttles. trace
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

/** SBUS scratchpad whose storage is one beat-wide memory per bank.
  *
  * testchipip's BankedScratchpad builds TLRAM with byte-granular writes, which
  * emits 32 separate 8-bit-wide arrays per bank (one per byte lane). On an
  * FPGA those can't use URAM efficiently (a 128K x 8 lane takes 32 URAM288,
  * 8 of 72 bits used), so an 8 MiB scratchpad needs ~2,048 RAMB36 or ~2,048
  * URAM -- more than an F2 has. Here TLRAM gets ECC granularity = beatBytes
  * with the identity code, so each bank is ONE (beatBytes*8)-bit-wide array
  * (mem_0): 4 MiB per bank = 128K x 256 bits = 128 URAM288 (4 wide x 32
  * deep). Sub-beat writes become read-modify-write inside TLRAM; weight/KV
  * streams write and read full beats.
  * Same layout as testchipip WithScratchpad(banks = n, subBanks = 1): banks
  * interleaved at cache-block stride, instance names bank / bank_N / ram.
  */
case class IrisStudySpadParams(base: BigInt, size: BigInt, banks: Int)
case object IrisStudySpadKey extends Field[Option[IrisStudySpadParams]](None)

class IrisStudySpadBank(address: AddressSet, beatBytes: Int, devOverride: MemoryDevice)(implicit p: Parameters)
    extends ClockSinkDomain(ClockSinkParameters())(p) {
  val xbar = TLXbar()
  val ram = LazyModule(new TLRAM(
    address = address,
    beatBytes = beatBytes,
    ecc = ECCParams(bytes = beatBytes),       // identity code: one beat-wide word, no ECC bits
    devOverride = Some(devOverride)) {
    override lazy val desiredName = "TLRAM_IrisStudySpad"
  })
  ram.node := TLFragmenter(beatBytes, p(freechips.rocketchip.subsystem.CacheBlockBytes), nameSuffix = Some("IrisStudySpad")) := TLBuffer() := xbar
  override lazy val desiredName = "IrisStudySpadBank"
}

case object IrisStudySpadInjector extends SubsystemInjector((p, baseSubsystem) => {
  p(IrisStudySpadKey).foreach { params =>
    implicit val q: Parameters = p
    val bus = baseSubsystem.locateTLBusWrapper(SBUS)
    val stripe = p(freechips.rocketchip.subsystem.CacheBlockBytes)
    val mask = (params.banks - 1) * stripe
    val device = new MemoryDevice
    (0 until params.banks).foreach { b =>
      val bank = LazyModule(new IrisStudySpadBank(
        AddressSet(params.base + stripe * b, params.size - 1 - mask), bus.beatBytes, device))
      bank.clockNode := bus.fixedClockNode
      bus.coupleTo(s"iris-study-spad-$b") { bank.xbar := bus { TLBuffer() } := _ }
    }
  }
})

class WithIrisStudyWideScratchpad(base: BigInt, size: BigInt, banks: Int = 2) extends Config((site, here, up) => {
  case IrisStudySpadKey => Some(IrisStudySpadParams(base, size, banks))
  case SubsystemInjectorKey => up(SubsystemInjectorKey) + IrisStudySpadInjector
})

/** WithIrisUncore (iris-cy) minus WithNoMemPort and the serial-TL manager port,
  * with the scratchpad sized by the study. The serial-TL client port (TSI
  * loading, HTIF) comes from AbstractConfig: 32-bit, external clock.
  */
class WithIrisStudyUncore(spadBytes: BigInt, l2KB: Int = 512, l2Banks: Int = 4, wideSpad: Boolean = true) extends Config(
  new freechips.rocketchip.subsystem.WithoutTLMonitors ++
  new testchipip.soc.WithOffchipAddressRange(AddressSet.misaligned(0x800000000L, 0x2000000000L)) ++

  // L2, as Iris
  new chipyard.config.WithInclusiveCacheInteriorBuffer ++
  new chipyard.config.WithInclusiveCacheExteriorBuffer ++
  new freechips.rocketchip.subsystem.WithInclusiveCache(nWays = 4, capacityKB = l2KB, outerLatencyCycles = 4) ++
  new freechips.rocketchip.subsystem.WithNBanks(l2Banks) ++

  // SBUS scratchpad: Iris's 2 banks at 0x5_8000_0000, study-sized. wideSpad:
  // one beat-wide memory per bank (FPGA URAM-friendly); otherwise testchipip's.
  (if (wideSpad) new WithIrisStudyWideScratchpad(IrisStudy.spadBase, spadBytes, banks = 2)
   else new Config(
     new testchipip.soc.WithNoScratchpadMonitors ++
     new testchipip.soc.WithScratchpad(base = IrisStudy.spadBase, size = spadBytes, banks = 2, partitions = 1))) ++

  new testchipip.soc.WithNoScratchpads ++   // drops AbstractConfig's MBUS scratchpad
  new chipyard.config.AbstractConfig
)

/** The mempress memcopy engine (mempress `dma` branch, RoCC custom2) on Rocket
  * hart 2 only: separate read and write L2 helpers, 32-B beats, 32 outstanding
  * requests each. Shuttle tiles also read BuildRoCC, so WithMultiRoCC scopes
  * it. Driver: sram-vs-bw-study/llm-bm/bench/dma_compare.c (mempress_copy).
  */
class WithIrisStudyMempress(rocketHart: Int) extends Config(
  new chipyard.config.WithMultiRoCC ++
  new chipyard.config.WithMultiRoCCFromBuildRoCC(rocketHart) ++
  new mempress.WithMemPress(maxStreams = 1, beatBytes = 32, useMemLoader = true)
)

/** The study chiplet (no harness/FireSim specifics). */
class IrisStudyConfig(
  spadBytes: BigInt = BigInt(8) << 20,
  memChannels: Int = 1,
  sbusNoC: Boolean = false,      // true: Iris's Constellation NoC (for the NoC findings)
  mempressDMA: Boolean = true,
  tilePortBytes: Int = 32,       // Shuttle tile (TCM) port; iris 0d5ab69 uses 16
  nShuttles: Int = 1,            // Iris has 2; the study FPGA build fits 1
  int8OPU: Boolean = true,       // VectorParams.opuParams (int8 VME) instead of opuMxParams (+MX/FP8)
  rocketVector: Boolean = false, // Saturn DMA vector unit on the Rocket tile (Iris has it)
  l2KB: Int = 512,               // FPGA-fit fallbacks may shrink the L2 (Iris: 512 KiB, 4 banks)
  l2Banks: Int = 4,
  wideSpad: Boolean = true       // beat-wide scratchpad memory (URAM-friendly); false = testchipip's
) extends Config(
  new chipyard.harness.WithAbsoluteFreqHarnessClockInstantiator ++

  // 1 GHz everywhere (tiles, buses, d2d), as the UCIe bridge configs
  new chipyard.WithUcieChipletFrequency(1000.0) ++

  new WithIrisStudyUcie ++
  new WithIrisStudyMemory(memChannels) ++
  (if (mempressDMA) new WithIrisStudyMempress(rocketHart = nShuttles) else Parameters.empty) ++

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
  (if (rocketVector) new saturn.rocket.WithRocketVectorUnit(256, 256, VectorParams.dmaParams) else Parameters.empty) ++
  new WithIrisRocketCore ++

  // ==================================
  // Shuttle tiles + Saturn/OPU (harts 0-1)
  // ==================================
  new saturn.shuttle.WithShuttleVectorUnit(256, 128, if (int8OPU) VectorParams.opuParams else VectorParams.opuMxParams) ++
  new shuttle.common.WithShuttleTileBeatBytes(tilePortBytes) ++   // iris 0d5ab69: 16, iris-cy: 32
  new shuttle.common.WithTCM(size = 128L << 10, banks = 2) ++
  new shuttle.common.WithShuttleTileBoundaryBuffers() ++
  new shuttle.common.WithL1ICacheWays(2) ++
  new shuttle.common.WithL1ICacheSets(64) ++
  new shuttle.common.WithL1DCacheWays(2) ++
  new shuttle.common.WithL1DCacheBanks(1) ++
  new shuttle.common.WithL1DCacheTagBanks(1) ++
  new shuttle.common.WithNShuttleCores(nShuttles) ++

  new WithIrisStudyUncore(spadBytes, l2KB, l2Banks, wideSpad)
)

/** Chipyard RTL sim (sims/vcs) of the study chiplet (crossbar SBUS, mempress),
  * both D2D ports tied off, memory on the harness's SimDRAM.
  */
class IrisStudyRTLConfig extends Config(
  new chipyard.harness.WithD2DTiedOff ++
  new IrisStudyConfig
)

/** Iris's interconnect as taped out (iris 0d5ab69): Constellation SBUS NoC and
  * 16-B Shuttle tile ports. Reproduces the NoC copy-bandwidth findings (iris
  * docs/NOC_BANDWIDTH.md).
  */
class IrisStudyNoCRTLConfig extends Config(
  new chipyard.harness.WithD2DTiedOff ++
  new IrisStudyConfig(sbusNoC = true, tilePortBytes = 16, nShuttles = 2, int8OPU = false, rocketVector = true, wideSpad = false)
)

/** IrisStudyNoCRTLConfig with a crossbar SBUS: same 16-B tile ports, so the
  * difference from IrisStudyNoCRTLConfig is the NoC alone.
  */
class IrisStudyXbar16RTLConfig extends Config(
  new chipyard.harness.WithD2DTiedOff ++
  new IrisStudyConfig(tilePortBytes = 16, nShuttles = 2, int8OPU = false, rocketVector = true, wideSpad = false)
)
