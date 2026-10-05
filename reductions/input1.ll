; Reduction 1: preserve an equality result proved positive (definitely true).

define i1 @equal_is_positive(i32 %value) {
entry:
  %zero = and i32 %value, 0
  %zero.again = mul i32 %zero, %value
  %irrelevant = add i32 %value, 17
  %result = icmp eq i32 %zero, %zero.again
  ret i1 %result
}

define i32 @unrelated1(i32 %value) {
entry:
  %noise = xor i32 %value, 91
  ret i32 %noise
}
