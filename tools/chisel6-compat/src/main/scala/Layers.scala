// Chisel 7's built-in layers, for sources compiled against Chisel 6.
//
// Chisel 7 predefines `chisel3.layers.Verification` (with `Assert`, `Assume`
// and `Cover` under it); Chisel 6 has the layer machinery but not these
// layers. Generators shared with Chisel 7 builds (the UCIe stack) put their
// assertions in them, so define them here, bound the way Chisel 7 binds them.
// Only compiled when building with Chisel 6 (see build.sbt).
package chisel3.layers

import chisel3.layer.{Convention, Layer}

object Verification extends Layer(Convention.Bind) {
  object Assert extends Layer(Convention.Bind)
  object Assume extends Layer(Convention.Bind)
  object Cover extends Layer(Convention.Bind)
}
