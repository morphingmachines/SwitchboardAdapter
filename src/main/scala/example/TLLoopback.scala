package switchboard

import org.chipsalliance.cde.config.Parameters

class TLLoopback(implicit p: Parameters) extends SwitchboardTLAdapter {

  override val nManagerParams = Seq(TLManagerPortParams(base = 0, size = 0x10000, beatBytes = 8))
  override val nClientParams  = Seq(TLClientPortParams(idBits = 4))

  (0 until nManagerParams.length).foreach(i => managers(i) := clients(i))

  lazy val module = new TLLoopbackImp(this)
}

class TLLoopbackImp(outer: TLLoopback) extends SwitchboardTLAdapterImp(outer) {}
