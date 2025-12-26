! { dg-do run }
! { dg-additional-options "-cpp" }

program test_minimal
  use openacc
  implicit none

  real :: a(100)
  integer :: i
  integer :: on_device

  a = 0.0
  on_device = 0

  !$acc parallel loop copy(a) reduction(max:on_device)
  do i = 1, 100
    if (acc_on_device (acc_device_not_host)) on_device = 1
    a(i) = real(i)
  end do
  !$acc end parallel loop

#if ACC_DEVICE_TYPE_metal
  if (on_device .ne. 1) stop 2
#endif

  if (abs (a(50) - 50.0) .gt. 0.001) stop 1
end program test_minimal

