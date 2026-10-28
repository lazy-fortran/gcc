! PR libgomp/123280, PR libgomp/96080
! Same as pr123280.f90, but using openacc_lib.h instead of the module.
! { dg-do run }
! { dg-options "-Warray-temporaries" }

program pr123280_2
  implicit none
  include "openacc_lib.h"

  real, allocatable, target :: values(:)
  real, pointer :: view(:)

  allocate(values(37))
  values = 1.0
  view => values

  !$acc enter data copyin(values)

  if (.not. acc_is_present(values)) stop 1
  call check_assumed_shape(values)
  call check_contiguous(values)

  if (.not. acc_is_present(view)) stop 4 ! { dg-bogus "Creating array temporary" }
  call check_pointer(view)
  call check_assumed_shape(view)

  !$acc exit data delete(values)

contains

  subroutine check_assumed_shape(x)
    real, intent(in) :: x(:)

    if (.not. acc_is_present(x)) stop 2 ! { dg-bogus "Creating array temporary" }
  end subroutine check_assumed_shape

  subroutine check_contiguous(x)
    real, intent(in), contiguous :: x(:)

    if (.not. acc_is_present(x)) stop 3
  end subroutine check_contiguous

  subroutine check_pointer(p)
    real, pointer, intent(in) :: p(:)

    if (.not. acc_is_present(p)) stop 5 ! { dg-bogus "Creating array temporary" }
  end subroutine check_pointer

end program pr123280_2
