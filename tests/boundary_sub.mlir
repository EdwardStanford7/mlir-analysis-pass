module {
  llvm.func @main(%argc: i32, %argv: !llvm.ptr) -> i32 {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %one = llvm.mlir.constant(1 : i32) : i32
    %small = llvm.mlir.constant(-5 : i8) : i8
    %minimum = llvm.mlir.constant(-128 : i8) : i8
    %decrement = llvm.mlir.constant(1 : i8) : i8
    %boundary = llvm.icmp "sgt" %argc, %one : i32
    llvm.cond_br %boundary, ^large, ^small
  ^large:
    llvm.br ^join(%minimum : i8)
  ^small:
    llvm.br ^join(%small : i8)
  ^join(%x: i8):
    %result = llvm.sub %x, %decrement : i8
    llvm.return %zero : i32
  }
}
