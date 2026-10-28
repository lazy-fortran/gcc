! { dg-do run }
!
! PR fortran/123252
! Test that scalar fields of a derived type are available on device
! when only an allocatable component is explicitly mapped.

program test_scalar_access
  implicit none

  type :: container_t
    real(8), allocatable :: arr(:)
    logical :: flag
    integer :: n
  end type container_t

  type(container_t) :: c
  integer :: i

  c%n = 10
  c%flag = .true.
  allocate(c%arr(c%n))
  c%arr = 0.0d0

  !$acc enter data copyin(c%arr)

  ! Access scalar fields c%n and c%flag inside parallel region
  !$acc parallel loop present(c%arr)
  do i = 1, c%n
    if (c%flag) then
      c%arr(i) = real(i, 8)
    end if
  end do

  !$acc update self(c%arr)
  !$acc exit data delete(c%arr)

  ! Verify results
  do i = 1, c%n
    if (abs(c%arr(i) - real(i, 8)) > 1.0d-10) stop 1
  end do

  deallocate(c%arr)
end program
