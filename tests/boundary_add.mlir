// No overflow flags: fixed-width LLVM addition wraps with defined behavior.
// No arguments selects a small operand; extra arguments select the boundary.
module {
  llvm.func @main(%argc: i32, %argv: !llvm.ptr) -> i32 {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %one = llvm.mlir.constant(1 : i32) : i32
    %small = llvm.mlir.constant(5 : i8) : i8
    %maximum = llvm.mlir.constant(127 : i8) : i8
    %increment = llvm.mlir.constant(1 : i8) : i8
    %boundary = llvm.icmp "sgt" %argc, %one : i32
    llvm.cond_br %boundary, ^large, ^small
  ^large:
    llvm.br ^join(%maximum : i8)
  ^small:
    llvm.br ^join(%small : i8)
  ^join(%x: i8):
    %result = llvm.add %x, %increment : i8
    llvm.return %zero : i32
  }
}
