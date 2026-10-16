module {
  llvm.func @main(%argc: i32, %argv: !llvm.ptr) -> i32 {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %one = llvm.mlir.constant(1 : i32) : i32
    %limit = llvm.mlir.constant(5 : i32) : i32
    llvm.br ^loop(%zero, %zero : i32, i32)
  ^loop(%i: i32, %sum: i32):
    %more = llvm.icmp "slt" %i, %limit : i32
    llvm.cond_br %more, ^body, ^exit
  ^body:
    %nextSum = llvm.add %sum, %i : i32
    %next = llvm.add %i, %one : i32
    llvm.br ^loop(%next, %nextSum : i32, i32)
  ^exit:
    llvm.return %zero : i32
  }
}
