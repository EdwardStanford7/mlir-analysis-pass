// The initial analysis recognizes constants. Other operations remain unknown.
module {
  llvm.func @transfers(%unknown: i32) {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %negative = llvm.mlir.constant(-2 : i32) : i32
    %positive = llvm.mlir.constant(3 : i32) : i32
    %add = llvm.add %unknown, %positive : i32
    llvm.return
  }
}
