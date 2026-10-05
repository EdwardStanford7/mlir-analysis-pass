; Reduction 3: propagate signs through several arithmetic operations and prove
; that the final multiplication is zero-or-negative.

define i32 @multiply_is_zero_or_negative(i32 %value) {
entry:
  %nonnegative = and i32 %value, 7
  %positive.a = add i32 %nonnegative, 1
  %negative = sub i32 0, %positive.a
  %positive.b = add i32 2, 3
  %zero.or.positive = sdiv i32 %positive.a, %positive.b
  %result = mul i32 %negative, %zero.or.positive
  %irrelevant = add i32 %value, 29
  ret i32 %result
}

define i32 @unrelated3(i32 %value) {
entry:
  %noise = xor i32 %value, 123
  ret i32 %noise
}
