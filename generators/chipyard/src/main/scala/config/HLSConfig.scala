package chipyard
import chisel3._
import freechips.rocketchip.diplomacy.{LazyModule, ValName}
import org.chipsalliance.cde.config.{Parameters, Config}
import testchipip.iceblk.{WithBlockDevice, BlockDeviceKey, BlockDeviceConfig}
import freechips.rocketchip.tile._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.system._
import freechips.rocketchip.system.DefaultConfig
import freechips.rocketchip.rocket._
import freechips.rocketchip.tilelink._     
import freechips.rocketchip.devices.tilelink._
import freechips.rocketchip._
import testchipip._

import sifive.blocks.devices.uart._
import java.io.File

import hls_rocc0_vadd_vadd.HLSrocc0_vadd_vaddControl
// import hls_tl0_vadd_tl_vadd.WithHLStl0_vadd_tl_vadd


class WithHLSRoCCExample extends Config((site, here, up) => {
    case BuildRoCC => Seq(
        (p: Parameters) => {
            val hls_rocc0_vadd_vadd = LazyModule(new HLSrocc0_vadd_vaddControl(OpcodeSet.custom0)(p))
            hls_rocc0_vadd_vadd
        },
        (p: Parameters) => {
            val translator = LazyModule(new TranslatorExample(OpcodeSet.custom3)(p))
            translator 
        }
    )
})

class HLSRocketConfig extends Config(
    //     new WithHLStl0_vadd_tl_vadd ++
 // Disable due to Error scala.Some cannot be cast to scala.Function1, and our accelerators currently do not have parameters  
    new WithHLSRoCCExample ++ 
    new freechips.rocketchip.subsystem.WithNBigCores(1) ++         // single rocket-core
    new chipyard.config.AbstractConfig)
