// See README.md for license details.

package switchboard

import org.chipsalliance.cde.config.Parameters
import org.chipsalliance.diplomacy.lazymodule.LazyModule

/** To run from a terminal shell
  * {{{
  * mill explorerTL.runMain explorerTL.explorerTLMain
  * }}}
  */

object lazyrtlMain extends App with emitrtl.LazyToplevel {
  val str = if (args.length == 0) "" else args(0)
  val lazyTop = str match {
    case "Loopback" => LazyModule(new TLLoopback()(Parameters.empty))
    case "Mem"      => LazyModule(new TLMem()(Parameters.empty))
    case _          => throw new Exception("Unknown Module Name!")
  }

  showModuleComposition(lazyTop)
  chisel2firrtl()
  firrtl2sv()
  genDiplomacyGraph()
}

object rtlMain extends App with emitrtl.Toplevel {
  val str = if (args.length == 0) "" else args(0)
  lazy val topModule = str match {
    case "Minimal" => new Minimal
    case _         => throw new Exception("Unknown Module Name!")
  }
  chisel2firrtl()
  firrtl2sv()
}
