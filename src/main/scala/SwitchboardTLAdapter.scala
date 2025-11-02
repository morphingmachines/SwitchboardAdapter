package switchboard

import chisel3._
import chisel3.util._
import freechips.rocketchip.diplomacy.{AddressSet, IdRange, RegionType, TransferSizes}
import freechips.rocketchip.tilelink.{
  TLClientNode,
  TLManagerNode,
  TLMasterParameters,
  TLMasterPortParameters,
  TLSlaveParameters,
  TLSlavePortParameters,
}
import org.chipsalliance.cde.config.Parameters
import org.chipsalliance.diplomacy.lazymodule._

abstract class SwitchboardTLAdapter(implicit p: Parameters) extends LazyModule {

  /** Each Switchboard payload carries either TLBundleA or TLBundleD transfer. A single transaction may contain multiple
    * transfers.
    *
    * Note that the actual Tilelink agent interface interface parameters must be smaller or equal to the specified value
    * in the [[SBTLBundleParameters]]
    */

  val nManagerParams: Seq[TLManagerPortParams]
  val nClientParams:  Seq[TLClientPortParams]

  lazy val managers = nManagerParams.map { i =>
    require(i.maxXferBytes <= SBConst.TLMaxTransferSz)
    require(i.beatBytes <= SBConst.TLBeatBytes)
    TLManagerNode(
      Seq(
        TLSlavePortParameters.v1(
          Seq(
            TLSlaveParameters.v1(
              address = AddressSet.misaligned(i.base, i.size),
              regionType = RegionType.UNCACHED,
              executable = i.executable,
              supportsPutFull = TransferSizes(1, i.maxXferBytes),
              supportsPutPartial = TransferSizes(1, i.maxXferBytes),
              supportsGet = TransferSizes(1, i.maxXferBytes),
              mayDenyGet = false,
              mayDenyPut = false,
            ),
          ),
          beatBytes = i.beatBytes,
        ),
      ),
    )
  }

  lazy val clients = nClientParams.map { i =>
    require(i.idBits <= SBConst.SBTLBundleParameters.sourceBits)
    TLClientNode(
      Seq(
        TLMasterPortParameters.v1(
          Seq(TLMasterParameters.v1(name = "SwitchboardWrapperMasterPort", sourceId = IdRange(0, 1 << i.idBits))),
        ),
      ),
    )
  }
  override val module: SwitchboardTLAdapterImp[SwitchboardTLAdapter]
}

abstract class SwitchboardTLAdapterImp[+L <: SwitchboardTLAdapter](outer: L) extends LazyModuleImp(outer) {

  val io = IO(new Bundle {
    val manager = Vec(
      outer.nManagerParams.length,
      new Bundle {
        val d = Flipped(new SBIO)
        val a = new SBIO
      },
    )

    val client = Vec(
      outer.nClientParams.length,
      new Bundle {
        val d = new SBIO
        val a = Flipped(new SBIO)
      },
    )
  })

  (0 until outer.nClientParams.length).foreach { i =>
    val (client_port, _) = outer.clients(i).out(0)
    val clientABuf       = Module(new Queue(client_port.a.bits.cloneType, 8))
    val clientDBuf       = Module(new Queue(client_port.d.bits.cloneType, 8))

    require(client_port.a.bits.data.getWidth <= SBConst.SBTLBundleParameters.dataBits)
    require(client_port.a.bits.address.getWidth <= SBConst.SBTLBundleParameters.addressBits)
    require(client_port.a.bits.size.getWidth <= SBConst.SBTLBundleParameters.sizeBits)
    require(client_port.a.bits.source.getWidth <= SBConst.SBTLBundleParameters.sourceBits)
    require(client_port.d.bits.sink.getWidth <= SBConst.SBTLBundleParameters.sinkBits)

    clientABuf.io.enq <> io.client(i).a.toTLA
    io.client(i).d.fromTLD(clientDBuf.io.deq)
    client_port.a <> clientABuf.io.deq
    clientDBuf.io.enq <> client_port.d
  }

  (0 until outer.nManagerParams.length).foreach { i =>
    val (manager_port, _) = outer.managers(i).in(0)
    require(manager_port.a.bits.address.getWidth <= SBConst.SBTLBundleParameters.addressBits)
    require(manager_port.a.bits.data.getWidth <= SBConst.SBTLBundleParameters.dataBits)
    require(manager_port.a.bits.size.getWidth <= SBConst.SBTLBundleParameters.sizeBits)
    require(manager_port.a.bits.source.getWidth <= SBConst.SBTLBundleParameters.sourceBits)
    require(manager_port.d.bits.sink.getWidth <= SBConst.SBTLBundleParameters.sinkBits)

    val managerABuf = Module(new Queue(manager_port.a.bits.cloneType, 8))
    val managerDBuf = Module(new Queue(manager_port.d.bits.cloneType, 8))

    io.manager(i).a.fromTLA(managerABuf.io.deq)

    managerDBuf.io.enq <> io.manager(i).d.toTLD

    manager_port.d <> managerDBuf.io.deq
    managerABuf.io.enq <> manager_port.a

  }

}
