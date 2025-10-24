package switchboard

import freechips.rocketchip.diplomacy.AddressSet
import org.chipsalliance.cde.config.Parameters
import org.chipsalliance.diplomacy.lazymodule._

class TLMem(implicit p: Parameters) extends LazyModule with SwitchboardTLAdapter {

  override val nManagerParams = Seq.empty
  override val nClientParams  = Seq(TLClientPortParams(idBits = 4))

  val ram = LazyModule(new freechips.rocketchip.tilelink.TLRAM(AddressSet(0, 0xffff), beatBytes = 4))
  ram.node := clients(0)

}
