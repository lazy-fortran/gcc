! { dg-do compile }
! { dg-options "-O2 -fopenacc -fdump-tree-omplower" }
!
! PR fortran/103276
! OpenACC ENTER DATA on a derived-type dummy argument must not take the
! address of the reference slot (which lives on the current stack frame).
! The mapping should use the reference value directly, not &var.

module m
  implicit none

  type :: t
    integer :: v
  end type t

contains

  subroutine copyin_scalar (var)
    type(t), intent(inout) :: var
    !$acc enter data copyin(var)
  end subroutine copyin_scalar

end module m

! Verify that the lowered code does not contain "&var" assignment to
! .omp_data_arr, which would indicate taking address of reference slot.
! { dg-final { scan-tree-dump-not "\\.omp_data_arr\\..*= &var" "omplower" } }
