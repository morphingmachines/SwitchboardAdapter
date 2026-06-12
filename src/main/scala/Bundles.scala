package switchboard

import chisel3._
import chisel3.reflect.DataMirror
import chisel3.util._
import freechips.rocketchip.diplomacy.AddressSet
import freechips.rocketchip.tilelink.{TLBundleA, TLBundleD, TLBundleParameters}

object SBConst {
  val DataWidth       = 416 // 416 bits Switchboard payload size, refer: https://github.com/zeroasiccorp/switchboard
  val TLBeatBytes     = math.pow(2, log2Floor(DataWidth / 8)).toInt
  val TLMaxTransferSz = 4096

  /** Maximum allowed value for a TileLink interconnect parameter. SB adapter for TileLink converts ant TL interface
    * fields to these sizes.
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
  val data = UInt(SBConst.DataWidth.W)
  val dest = UInt(32.W)
  val last = Bool()
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
    val v      = Cat(data, mask, address, source, ctrl32)
    require(v.getWidth <= SBConst.DataWidth)
    v.pad(SBConst.DataWidth)
  }

  def unpack(d: UInt) = {
    require(d.getWidth == SBConst.DataWidth)
    val x = Wire(new SwitchboardTLBundleA)
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
    val x = Wire(new SwitchboardTLBundleA)
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
    val v      = Cat(data, denied, sink, source, ctrl32)
    require(v.getWidth <= SBConst.DataWidth)
    v.pad(SBConst.DataWidth)
  }

  def unpack(d: UInt) = {
    require(d.getWidth == SBConst.DataWidth)
    val x = Wire(new SwitchboardTLBundleD)
    x.opcode  := d(7, 0)
    x.param   := d(15, 8)
    x.size    := d(23, 16)
    x.corrupt := d(31, 24)
    x.source  := d(63, 32)
    x.sink    := d(95, 64)
    x.denied  := d(103, 96)
    x.data    := d(359, 104)
    x
  }

  def fromTLD(d: TLBundleD) = {
    val x = Wire(new SwitchboardTLBundleD)
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
    x.sink    := sink
    x.corrupt := corrupt
    x.denied  := denied
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

  def connect_recv(x: DecoupledIO[UInt]) = {
    require(x.bits.getWidth <= SBConst.DataWidth)
    require(DataMirror.directionOf(x.bits) == ActualDirection.Input)
    data    := x.bits.pad(SBConst.DataWidth)
    valid   := x.valid
    x.ready := ready
    // NOTE: current implementation assumes single-beat and single-destination
    last := 1.U
    dest := 0.U
  }

  def connect_send(x: DecoupledIO[UInt]) = {
    require(x.bits.getWidth <= SBConst.DataWidth)
    require(DataMirror.directionOf(x.bits) == ActualDirection.Output)
    x.valid := valid
    x.bits  := data.take(x.bits.getWidth)
    ready   := x.ready
  }

  def fromTLA(x: DecoupledIO[TLBundleA]) = {
    // when(x.fire) {
    //  printf(cf"TLA2SB: ${x.bits}\n")
    // }
    valid   := x.valid
    x.ready := ready
    data    := (new SwitchboardTLBundleA).fromTLA(x.bits).pack
    // NOTE: current implementation assumes single-beat and single-destination
    dest := 0.U
    last := 1.U
  }

  def toTLA: DecoupledIO[TLBundleA] = {
    val x = Wire(Decoupled(new TLBundleA(SBConst.SBTLBundleParameters)))
    // when(x.fire) {
    //  printf(cf"SB2TLA: ${x.bits}\n")
    // }
    x.valid := valid
    ready   := x.ready
    x.bits  := (new SwitchboardTLBundleA).unpack(data).toTLA
    x
  }

  def fromTLD(x: DecoupledIO[TLBundleD]) = {
    // when(x.fire) {
    //  printf(cf"TLD2SB: ${x.bits}\n")
    // }
    valid   := x.valid
    x.ready := ready
    data    := (new SwitchboardTLBundleD).fromTLD(x.bits).pack
    // NOTE: current implementation assumes single-beat and single-destination
    dest := 0.U
    last := 1.U
  }

  def toTLD: DecoupledIO[TLBundleD] = {
    val x = Wire(Decoupled(new TLBundleD(SBConst.SBTLBundleParameters)))
    // when(x.fire) {
    //  printf(cf"SB2TLD: ${x.bits}\n")
    // }
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
  hasAMO:       Boolean = false,
)

case class TLClientPortParams(
  idBits:     Int,
  visibility: Seq[AddressSet] = Seq(AddressSet(0, ~0)),
)
