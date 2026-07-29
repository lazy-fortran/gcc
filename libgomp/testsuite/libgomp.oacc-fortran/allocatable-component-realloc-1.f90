! { dg-do run }
! { dg-skip-if "" { *-*-* } { "*" } { "-DACC_MEM_SHARED=0" } }
!
! Keep the component mapping out of the first compute construct.  This order
! exposed a stale descriptor mapping during the next allocation cycle.

module m
  use iso_fortran_env, only: dp => real64
  implicit none
  type :: t
    real(dp), allocatable :: c(:)
  end type
end module

program p
  use m
  implicit none
  type(t) :: s
  real(dp), allocatable :: w1(:,:,:), w2(:,:,:)
  integer :: i, iter

  do iter = 1, 3
    allocate(s%c(8), w1(8,8,0:1), w2(8,8,0:1))

    s%c = 0.0_dp
    w1 = 1.0_dp
    w2 = 2.0_dp

    !$acc enter data create(s%c, w2) copyin(w1)

    !$acc parallel loop present(w1, w2)
    do i = 1, 8
      w2(1,i,0) = w1(1,i,0)
    end do

    !$acc parallel loop present(w1, s%c)
    do i = 1, 8
      s%c(i) = w1(1,i,0)
    end do

    !$acc exit data copyout(s%c) delete(w1, w2)

    do i = 1, 8
      if (abs(s%c(i) - 1.0_dp) > 1.0e-10_dp) stop 1
    end do

    deallocate(w1, w2, s%c)
  end do
end program
