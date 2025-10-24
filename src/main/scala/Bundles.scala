package switchboard

import chisel3._
import chisel3.util._
import freechips.rocketchip.tilelink.{TLBundleA, TLBundleD, TLBundleParameters}

object SBConst {
  val BeatBytes       = 52 // Switchboard payload size
  val DataWidth       = BeatBytes * 8
  val TLBeatBytes     = math.pow(2, log2Floor(BeatBytes)).toInt
  val TLMaxTransferSz = 2048

  /** Maximum possible value for a TileLink interconnect parameter.
    */
  val SBTLBundleParameters = TLBundleParameters(
    addressBits = 64,
    dataBits = 256,
    sourceBits = 32,
    sinkBits = 32,
    sizeBits = 4,
    echoFields = Seq.empty,
    requestFields = Seq.empty,
    responseFields = Seq.empty,
    hasBCE = false,
  )
}

class SwitchboardIfc extends Bundle {
  val data = Output(UInt(SBConst.DataWidth.W))
  val dest = Output(UInt(32.W))
  val last = Output(Bool())
}

class SwitchboardTLBundleA extends Bundle {
  val opcode  = UInt(8.W)
  val param   = UInt(8.W)
  val size    = UInt(8.W)
  val source  = UInt(32.W)
  val address = UInt(64.W)
  val mask    = UInt(32.W)
  val corrupt = UInt(8.W)
  val data    = UInt(256.W)

  def pack: UInt = {
    val ctrl32 = Cat(corrupt, size, param, opcode)
    Cat(data, mask, address, source, ctrl32)
  }

  def unpack(d: UInt) = {
    val x = Wire(this.cloneType)
    x.opcode  := d(7, 0)
    x.param   := d(15, 8)
    x.size    := d(23, 16)
    x.corrupt := d(31, 24)
    x.source  := d(63, 32)
    x.address := d(127, 64)
    x.mask    := d(159, 128)
    x.data    := d(415, 160)
    x
  }

  def fromTLA(d: TLBundleA) = {
    val x = Wire(this.cloneType)
    x.opcode  := d.opcode
    x.param   := d.param
    x.size    := d.size
    x.source  := d.source
    x.address := d.address
    x.mask    := d.mask
    x.corrupt := d.corrupt
    x.data    := d.data
    x
  }

  def toTLA: TLBundleA = {
    val x = Wire(
      new TLBundleA(SBConst.SBTLBundleParameters),
    )

    x.opcode  := opcode
    x.param   := param
    x.size    := size
    x.source  := source
    x.address := address
    x.mask    := mask
    x.corrupt := corrupt
    x.data    := data
    x
  }
}

class SwitchboardTLBundleD extends Bundle {
  val opcode  = UInt(8.W)
  val param   = UInt(8.W)
  val size    = UInt(8.W)
  val corrupt = UInt(8.W)
  val source  = UInt(32.W)
  val sink    = UInt(32.W)
  val denied  = UInt(8.W)
  val data    = UInt(256.W)

  def pack: UInt = {
    val ctrl32 = Cat(corrupt, size, param, opcode)
    Cat(data, denied.pad(32), sink, source, ctrl32)
  }

  def unpack(d: UInt) = {
    val x = Wire(this.cloneType)
    x.opcode  := d(7, 0)
    x.param   := d(15, 8)
    x.size    := d(23, 16)
    x.corrupt := d(31, 24)
    x.source  := d(63, 32)
    x.sink    := d(95, 64)
    x.denied  := d(103, 96)
    x.data    := d(383, 128)
    x
  }

  def fromTLD(d: TLBundleD) = {
    val x = Wire(this.cloneType)
    x.opcode  := d.opcode
    x.param   := d.param
    x.size    := d.size
    x.source  := d.source
    x.sink    := d.sink
    x.corrupt := d.corrupt
    x.denied  := d.denied
    x.data    := d.data
    x
  }

  def toTLD: TLBundleD = {
    val x = Wire(
      new TLBundleD(SBConst.SBTLBundleParameters),
    )
    x.opcode  := opcode
    x.param   := param
    x.size    := size
    x.source  := source
    x.sink    := source
    x.corrupt := corrupt
    x.denied  := corrupt
    x.data    := data
    x
  }
}

class SBIO extends Bundle {
  val data  = Output(UInt(SBConst.DataWidth.W))
  val dest  = Output(UInt(32.W))
  val last  = Output(Bool())
  val valid = Output(Bool())
  val ready = Input(Bool())

  def fromTLA(x: DecoupledIO[TLBundleA]) = {
    when(x.fire) {
      printf(cf"TLA2SB: ${x.bits}\n")
    }
    valid   := x.valid
    x.ready := ready
    data    := (new SwitchboardTLBundleA).fromTLA(x.bits).pack
    dest    := 0.U
    last    := 1.U
  }

  def toTLA: DecoupledIO[TLBundleA] = {
    val x = Wire(Decoupled(new TLBundleA(SBConst.SBTLBundleParameters)))
    when(x.fire) {
      printf(cf"SB2TLA: ${x.bits}\n")
    }
    x.valid := valid
    ready   := x.ready
    x.bits  := (new SwitchboardTLBundleA).unpack(data).toTLA
    x
  }

  def fromTLD(x: DecoupledIO[TLBundleD]) = {
    when(x.fire) {
      printf(cf"TLD2SB: ${x.bits}\n")
    }
    valid   := x.valid
    x.ready := ready
    data    := (new SwitchboardTLBundleD).fromTLD(x.bits).pack
    dest    := 0.U
    last    := 1.U
  }

  def toTLD: DecoupledIO[TLBundleD] = {
    val x = Wire(Decoupled(new TLBundleD(SBConst.SBTLBundleParameters)))
    when(x.fire) {
      printf(cf"SB2TLD: ${x.bits}\n")
    }
    x.valid := valid
    ready   := x.ready
    x.bits  := (new SwitchboardTLBundleD).unpack(data).toTLD
    x
  }
}

case class TLManagerPortParams(
  base:         BigInt,
  size:         BigInt,
  beatBytes:    Int,
  maxXferBytes: Int = 256,
  executable:   Boolean = true,
)

case class TLClientPortParams(
  idBits: Int,
)
