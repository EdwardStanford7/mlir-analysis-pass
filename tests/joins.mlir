module {
  llvm.func @main(%argc: i32, %argv: !llvm.ptr) -> i32 {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %one = llvm.mlir.constant(1 : i32) : i32
    %negative = llvm.mlir.constant(-4 : i32) : i32
    %positive = llvm.mlir.constant(4 : i32) : i32
    %noArgs = llvm.icmp "eq" %argc, %one : i32
    llvm.cond_br %noArgs, ^zeroPath, ^negativePath
  ^zeroPath:
    llvm.br ^nonpositive(%zero : i32)
  ^negativePath:
    llvm.br ^nonpositive(%negative : i32)
  ^nonpositive(%x: i32):
    %negated = llvm.sub %zero, %x : i32
    llvm.cond_br %noArgs, ^positivePath, ^otherZeroPath
  ^positivePath:
    llvm.br ^nonnegative(%positive : i32)
  ^otherZeroPath:
    llvm.br ^nonnegative(%zero : i32)
  ^nonnegative(%y: i32):
    %product = llvm.mul %x, %y : i32
    %sum = llvm.add %negated, %y : i32
    llvm.return %zero : i32
  }
}
