! { dg-do compile }
! { dg-compile-aux-modules "pr126485-1.f90" }
!
! PR fortran/126485
!
! The inferred interface for an external procedure has artificial dummies.

subroutine pass_one(sample)
  use c_address_provider, only : c_loc
  implicit none
  real, target :: sample

  call accept_pointer(c_loc(sample))
end subroutine pass_one

subroutine pass_two(sample)
  use c_address_provider, only : c_loc
  implicit none
  real, target :: sample

  call accept_pointer(c_loc(sample))
end subroutine pass_two

! { dg-final { cleanup-modules "c_address_provider" } }
