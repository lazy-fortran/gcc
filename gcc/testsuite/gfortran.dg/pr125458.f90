! { dg-do compile }
! { dg-options "-frange-check" }

program constant_overflow
  implicit none

  print *, 50000 * 50000 - 1000000000 ! { dg-error "Arithmetic overflow" }
  print *, 2000000000 + 2000000000 ! { dg-error "Arithmetic overflow" }
end program constant_overflow
