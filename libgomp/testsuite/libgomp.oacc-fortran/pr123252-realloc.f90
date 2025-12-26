! { dg-do run }
!
! PR fortran/123252
! Test that allocatable components can be unmapped, deallocated,
! reallocated, and remapped without errors.  Scalar fields are
! mapped on enter data and unmapped on exit data to keep the
! structure element mapping state consistent.

program test_realloc_component
  use openacc
  implicit none

  type :: container_t
    integer :: scalar_field
    real(8), allocatable :: data(:)
  end type

  type(container_t), save :: c
  integer :: iter, n, i

  n = 10
  do iter = 1, 3
    ! Cleanup from previous iteration
    if (allocated(c%data)) then
      if (acc_is_present(c%data)) then
        !$acc exit data delete(c%data)
      end if
      deallocate(c%data)
    end if

    ! Set scalar and allocate component
    c%scalar_field = iter
    allocate(c%data(n))

    ! Map component to device - must work on all iterations
    !$acc enter data create(c%data)

    !$acc parallel loop present(c%data)
    do i = 1, n
      c%data(i) = real(i + iter, 8)
    end do

    !$acc update self(c%data)
    !$acc exit data delete(c%data)

    if (abs(c%data(1) - real(1 + iter, 8)) > 1.0d-10) stop 1
  end do
end program
