! PR libgomp/123280, PR libgomp/96080
! { dg-do run }
! { dg-skip-if "" { *-*-* } { "*" } { "-DACC_MEM_SHARED=0" } }
!
! Test that acc_is_present works correctly for assumed-shape dummy arguments
! and Fortran pointers. Before the fix, these cases would create a copy
! (due to the contiguous attribute), causing the lookup to fail.

program pr123280
  use openacc
  implicit none

  real, allocatable, target :: arr(:)
  real, pointer :: ptr(:)
  integer, parameter :: n = 100

  allocate(arr(n))
  arr = 1.0
  ptr => arr

  !$acc enter data copyin(arr)

  ! Test 1: Direct check should pass
  if (.not. acc_is_present(arr)) stop 1

  ! Test 2: Check via subroutine with assumed-shape should pass
  call check_assumed_shape(arr)

  ! Test 3: Check via subroutine with explicit contiguous should pass
  call check_contiguous(arr)

  ! Test 4: Check pointer to mapped target (PR 96080)
  if (.not. acc_is_present(ptr)) stop 4

  ! Test 5: Check pointer in subroutine
  call check_pointer(ptr)

  ! Test 6: Check pointer passed as assumed-shape
  call check_assumed_shape(ptr)

  !$acc exit data delete(arr)

contains

  subroutine check_assumed_shape(x)
    real, intent(in) :: x(:)

    ! This used to fail because gfortran created a copy of the array
    ! to satisfy the contiguous requirement in acc_is_present_array_h.
    if (.not. acc_is_present(x)) stop 2
  end subroutine check_assumed_shape

  subroutine check_contiguous(x)
    real, intent(in), contiguous :: x(:)

    ! With explicit contiguous on the dummy, no copy should be needed.
    if (.not. acc_is_present(x)) stop 3
  end subroutine check_contiguous

  subroutine check_pointer(p)
    real, pointer, intent(in) :: p(:)

    ! Pointer argument should detect mapped target (PR 96080).
    if (.not. acc_is_present(p)) stop 5
  end subroutine check_pointer

end program pr123280

