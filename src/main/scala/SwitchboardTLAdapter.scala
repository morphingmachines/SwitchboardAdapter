package switchboard

import chisel3._
import chisel3.util._
import freechips.rocketchip.diplomacy.{AddressSet, IdRange, RegionType, TransferSizes}
import freechips.rocketchip.tilelink.{
  TLBundleA,
  TLBundleD,
  TLClientNode,
  TLManagerNode,
  TLMasterParameters,
  TLMasterPortParameters,
  TLSlaveParameters,
  TLSlavePortParameters,
}
import org.chipsalliance.cde.config.Parameters
import org.chipsalliance.diplomacy.ValName
import org.chipsalliance.diplomacy.lazymodule._

abstract class SwitchboardTLAdapter(implicit p: Parameters) extends LazyModule {

  /** Each Switchboard payload carries either TLBundleA or TLBundleD transfer. A single transaction may contain multiple
    * transfers.
    *
    * Note that the actual Tilelink agent interface interface parameters must be smaller or equal to the specified value
    * in the [[SBTLBundleParameters]]
    */

  val nManagerParams: Seq[TLManagerPortParams] = Seq.empty
  val nClientParams:  Seq[TLClientPortParams]  = Seq.empty

  lazy val managers = nManagerParams.zipWithIndex.map { case (i, index) =>
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
          minLatency = 1,
        ),
      ),
    )(ValName(s"sb_Manager_$index"))
  }

  lazy val clients = nClientParams.zipWithIndex.map { case (i, index) =>
    require(i.idBits <= SBConst.SBTLBundleParameters.sourceBits)
    TLClientNode(
      Seq(
        TLMasterPortParameters.v1(
          Seq(
            TLMasterParameters.v1(
              name = "SwitchboardWrapperMasterPort",
              sourceId = IdRange(0, 1 << i.idBits),
              visibility = i.visibility,
            ),
          ),
        ),
      ),
    )(ValName(s"sb_client_$index"))
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

  private def validateAChannel(a: TLBundleA): Unit = {
    require(a.data.getWidth <= SBConst.SBTLBundleParameters.dataBits)
    require(a.address.getWidth <= SBConst.SBTLBundleParameters.addressBits)
    require(a.size.getWidth <= SBConst.SBTLBundleParameters.sizeBits)
    require(a.source.getWidth <= SBConst.SBTLBundleParameters.sourceBits)
  }

  private def validateDChannel(d: TLBundleD): Unit =
    require(d.sink.getWidth <= SBConst.SBTLBundleParameters.sinkBits)

  (0 until outer.nClientParams.length).foreach { i =>
    val (client_port, _) = outer.clients(i).out(0)

    // Client side: Outgoing A channel is forwarded to switchboard and incoming D channel is received.
    validateAChannel(client_port.a.bits)
    validateDChannel(client_port.d.bits)

    val clientABuf = Module(new Queue(client_port.a.bits.cloneType, 8))
    val clientDBuf = Module(new Queue(client_port.d.bits.cloneType, 8))

    clientABuf.io.enq <> io.client(i).a.toTLA
    io.client(i).d.fromTLD(clientDBuf.io.deq)
    client_port.a <> clientABuf.io.deq
    clientDBuf.io.enq <> client_port.d
  }

  (0 until outer.nManagerParams.length).foreach { i =>
    val (manager_port, _) = outer.managers(i).in(0)
    // Manager side: incoming A channel is received from switchboard and outgoing D channel is returned.
    validateAChannel(manager_port.a.bits)
    validateDChannel(manager_port.d.bits)

    val managerABuf = Module(new Queue(manager_port.a.bits.cloneType, 8))
    val managerDBuf = Module(new Queue(manager_port.d.bits.cloneType, 8))

    io.manager(i).a.fromTLA(managerABuf.io.deq)

    managerDBuf.io.enq <> io.manager(i).d.toTLD

    manager_port.d <> managerDBuf.io.deq
    managerABuf.io.enq <> manager_port.a

  }

}
