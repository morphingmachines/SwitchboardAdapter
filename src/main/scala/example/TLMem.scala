package switchboard

import freechips.rocketchip.diplomacy.AddressSet
import org.chipsalliance.cde.config.Parameters
import org.chipsalliance.diplomacy.lazymodule._

class TLMem(implicit p: Parameters) extends SwitchboardTLAdapter {

  override val nManagerParams = Seq.empty
  override val nClientParams  = Seq(TLClientPortParams(idBits = 4))

  val ram0 = LazyModule(new freechips.rocketchip.tilelink.TLRAM(AddressSet(0, 0xffff), beatBytes = 4))
  val ram1 = LazyModule(new freechips.rocketchip.tilelink.TLRAM(AddressSet(0x8000_0000L, 0xffff), beatBytes = 4))
  val xbar = freechips.rocketchip.tilelink.TLXbar()
  ram0.node := xbar := clients(0)
  ram1.node := xbar

  lazy val module = new TLMemImp(this)
}

class TLMemImp(outer: TLMem) extends SwitchboardTLAdapterImp(outer) {}
