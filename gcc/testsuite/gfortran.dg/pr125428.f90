! { dg-do run }
!
! PR fortran/125428
!
! Finalization must use the already evaluated left-hand side, including the
! initialization code generated for a component array temporary.

module final_lhs_m
  implicit none

  type :: cargo_t
    integer :: number
  contains
    final :: count_finalization
  end type

  type :: bin_t
    type(cargo_t), allocatable :: cargo
    integer :: priority
  end type

  type :: rack_t
    type(cargo_t) :: cargo(3)
    integer :: cursor
  end type

  integer :: finalizations

contains

  subroutine count_finalization(cargo)
    type(cargo_t), intent(inout) :: cargo

    finalizations = finalizations + 1
  end subroutine

  integer function select_bin(priority)
    integer, intent(inout) :: priority(:)

    select_bin = minloc(priority, dim=1)
    priority(select_bin) = 99
  end function

  subroutine store(bins, cargo)
    class(bin_t), intent(inout) :: bins(:)
    type(cargo_t), intent(in) :: cargo

    bins(select_bin(bins%priority))%cargo = cargo
  end subroutine

  integer function advance(cursor)
    integer, intent(inout) :: cursor

    advance = cursor
    cursor = mod(cursor, 3) + 1
  end function

  subroutine replace(rack, cargo)
    type(rack_t), intent(inout) :: rack
    type(cargo_t), intent(in) :: cargo

    rack%cargo(advance(rack%cursor)) = cargo
  end subroutine

end module

program final_lhs
  use final_lhs_m
  implicit none

  type(bin_t), allocatable :: bins(:)
  type(cargo_t) :: incoming
  type(rack_t) :: rack
  integer :: i

  allocate (bins(4))
  do i = 1, size(bins)
    allocate (bins(i)%cargo)
    bins(i)%cargo%number = -i
    bins(i)%priority = 10 * i
  end do

  incoming%number = 77
  finalizations = 0
  call store(bins, incoming)

  if (finalizations /= 1) error stop 1
  if (bins(1)%priority /= 99) error stop 2
  if (bins(2)%priority /= 20) error stop 3
  if (bins(1)%cargo%number /= 77) error stop 4

  rack%cursor = 2
  rack%cargo%number = [-1, -2, -3]
  incoming%number = 88
  finalizations = 0
  call replace(rack, incoming)

  if (rack%cursor /= 3) error stop 5
  if (finalizations /= 1) error stop 6
  if (rack%cargo(2)%number /= 88) error stop 7
end program
