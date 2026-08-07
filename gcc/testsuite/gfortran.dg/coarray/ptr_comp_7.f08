! { dg-do compile }
! { dg-options "-fcoarray=lib -Wall" }

! PR fortran/125113 - shmem CAF: a coarray data-pointer component associated
! with a non-coarray local target cannot be read from a remote image.
! The statically obvious cases should be diagnosed by -Wcoarray-ptr-target.

program ptr_comp_7
  type :: box
    integer, pointer :: data(:)
    integer, pointer :: scalar
  end type
  type(box), allocatable :: buffer[:]
  integer, allocatable, target :: src(:)
  integer, target :: dest

  allocate (buffer[*], src(4))

  buffer%data => src      ! { dg-warning "Pointer component of a coarray" }
  buffer%scalar => dest   ! { dg-warning "Pointer component of a coarray" }
end program ptr_comp_7
