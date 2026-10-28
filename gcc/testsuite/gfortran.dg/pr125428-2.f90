! { dg-do run }
!
! PR fortran/125428
!
! Self-assignment of a finalizable scalar derived type skips finalization.
! The evaluated left-hand side, including the scalar reallocation of an
! allocatable component, must still be emitted on that path.

module final_lhs_2_m
  implicit none

  type :: cargo_t
    integer :: number
  contains
    final :: count_finalization
  end type

  type :: bin_t
    type(cargo_t), allocatable :: cargo
  end type

  type :: rack_t
    type(cargo_t) :: cargo(3)
  end type

  integer :: finalizations

contains

  subroutine count_finalization(cargo)
    type(cargo_t), intent(inout) :: cargo

    finalizations = finalizations + 1
  end subroutine

  pure integer function pick(i)
    integer, intent(in) :: i

    pick = 4 - i
  end function

end module

program final_lhs_2
  use final_lhs_2_m
  implicit none

  type(bin_t), allocatable :: bins(:)
  type(rack_t) :: rack
  integer :: i, k

  allocate (bins(3))
  do i = 1, size(bins)
    allocate (bins(i)%cargo)
    bins(i)%cargo%number = 10 * i
  end do

  finalizations = 0
  k = 2
  bins(k)%cargo = bins(k)%cargo
  if (.not. allocated (bins(2)%cargo)) error stop 1
  if (bins(2)%cargo%number /= 20) error stop 2

  bins(pick(1))%cargo = bins(pick(1))%cargo
  if (.not. allocated (bins(3)%cargo)) error stop 3
  if (bins(3)%cargo%number /= 30) error stop 4

  rack%cargo%number = [1, 2, 3]
  rack%cargo(pick(2)) = rack%cargo(pick(2))
  rack%cargo(k) = rack%cargo(k)
  if (any (rack%cargo%number /= [1, 2, 3])) error stop 5
  if (bins(1)%cargo%number /= 10) error stop 6
end program
