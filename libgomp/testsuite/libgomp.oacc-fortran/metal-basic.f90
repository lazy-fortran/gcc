! { dg-do run { target openacc_metal_selected } }

! Basic Metal OpenACC integration test

program metal_basic
    use openacc
    implicit none
    integer :: i, a(100)

    ! Verify Metal device is selected
    if (acc_get_device_type() /= acc_device_metal) stop 1

    ! Simple parallel loop test
    !$acc parallel loop
    do i = 1, 100
        a(i) = i * 2
    end do
    !$acc end parallel loop

    ! Verify results
    if (a(50) /= 100) stop 2

end program metal_basic
