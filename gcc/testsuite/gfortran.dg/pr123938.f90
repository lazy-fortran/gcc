! { dg-do compile }
! { dg-options "-fdump-tree-original" }
!
! PR fortran/123938
! The finalization of a function result passed to defined assignment must
! deallocate allocatable components of its polymorphic component.

module temporary_cleanup
  implicit none
  private
  public :: package_t

  type, abstract :: item_t
  end type item_t

  type, extends(item_t) :: buffer_t
    integer, allocatable :: value
  end type buffer_t

  type :: container_t
    private
  contains
    procedure :: assign_container
    generic :: assignment(=) => assign_container
  end type container_t

  type, extends(container_t) :: package_t
    private
    class(item_t), allocatable :: payload
  end type package_t

  interface package_t
    module procedure make_package
  end interface package_t

contains

  subroutine assign_container(lhs, rhs)
    class(container_t), intent(out) :: lhs
    class(container_t), intent(in) :: rhs
  end subroutine assign_container

  function make_package() result(package)
    type(package_t) :: package

    allocate(buffer_t :: package%payload)
    select type (payload => package%payload)
    type is (buffer_t)
      allocate(payload%value, source=42)
    end select
  end function make_package

end module temporary_cleanup

program test_temporary_cleanup
  use temporary_cleanup, only: package_t
  implicit none

  type(package_t) :: destination

  destination = package_t()
end program test_temporary_cleanup

! Two references dispatch finalization of the constructed package.  The other
! two are in the generated final subroutine and do not clean up the temporary.
! { dg-final { scan-tree-dump-times "payload._vptr->_final" 4 "original" } }
