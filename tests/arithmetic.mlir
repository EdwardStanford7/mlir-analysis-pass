module {
  llvm.func @main(%argc: i32, %argv: !llvm.ptr) -> i32 {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %positive = llvm.mlir.constant(7 : i32) : i32
    %negative = llvm.mlir.constant(-3 : i32) : i32
    %sum = llvm.add %positive, %negative : i32
    %difference = llvm.sub %negative, %positive : i32
    %product = llvm.mul %negative, %negative : i32
    %quotient = llvm.sdiv %negative, %positive : i32
    %negativeQuotient = llvm.sdiv %positive, %negative : i32
    %masked = llvm.and %argc, %positive : i32
    %cleared = llvm.and %argc, %zero : i32
    %equal = llvm.icmp "eq" %cleared, %zero : i32
    %greater = llvm.icmp "sgt" %positive, %negative : i32
    %booleanAsInt = llvm.zext %equal : i1 to i32
    %scaled = llvm.mul %masked, %negative : i32
    llvm.return %zero : i32
  }
}
