! { dg-do run }
! { dg-skip-if "" { *-*-* } { "*" } { "-DACC_MEM_SHARED=0" } }
!
! Like allocatable-component-realloc-1.f90, but with two allocatable
! components of the same derived type mapped by one directive.  Each
! descriptor must be counted once on enter data, so that exit data unmaps
! it and the next allocation cycle does not see a stale mapping.

module m
  use iso_fortran_env, only: dp => real64
  implicit none
  type :: t
    real(dp), allocatable :: a(:)
    real(dp), allocatable :: b(:)
  end type
end module

program p
  use m
  use openacc
  implicit none
  type(t) :: s
  real(dp), allocatable :: w1(:,:,:)
  integer :: i, iter

  do iter = 1, 3
    allocate(s%a(8), s%b(8), w1(8,8,0:1))

    s%a = 0.0_dp
    s%b = 0.0_dp
    w1 = 1.0_dp

    !$acc enter data create(s%a, s%b) copyin(w1)

    !$acc parallel loop present(w1, s%a, s%b)
    do i = 1, 8
      s%a(i) = w1(1,i,0)
      s%b(i) = 2.0_dp * w1(1,i,0)
    end do

    !$acc exit data copyout(s%a, s%b) delete(w1)

    if (acc_is_present(s%a)) stop 2
    if (acc_is_present(s%b)) stop 3

    do i = 1, 8
      if (abs(s%a(i) - 1.0_dp) > 1.0e-10_dp) stop 1
      if (abs(s%b(i) - 2.0_dp) > 1.0e-10_dp) stop 1
    end do

    deallocate(w1, s%a, s%b)
  end do
end program
