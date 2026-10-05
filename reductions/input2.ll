; Reduction 2: preserve a signed greater-than result proved positive.

define i1 @greater_than_is_positive(i32 %value) {
entry:
  %nonnegative = and i32 %value, 7
  %positive = add i32 1, 1
  %negative = sub i32 0, %positive
  %irrelevant = mul i32 %value, 23
  %result = icmp sgt i32 %nonnegative, %negative
  ret i1 %result
}

define i32 @unrelated2(i32 %value) {
entry:
  %noise = or i32 %value, 7
  ret i32 %noise
}
