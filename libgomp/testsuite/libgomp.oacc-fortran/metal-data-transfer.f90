! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_accel_selected } }
! { dg-additional-options "-fopenacc" }

! Test basic Metal data transfer operations (enter data, exit data)

program metal_data_transfer
    use openacc
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 1024
    real(dp) :: a(n), b(n)
    integer :: i, errors

    ! Initialize on host
    do i = 1, n
        a(i) = real(i, dp)
        b(i) = 0.0_dp
    end do

    ! Test copyin: transfer data to device
    !$acc enter data copyin(a)

    ! Verify a is present on device
    if (.not. acc_is_present(a)) then
        print *, "Error: array a not present on device after enter data"
        stop 1
    end if

    ! Test create: allocate space on device
    !$acc enter data create(b)

    if (.not. acc_is_present(b)) then
        print *, "Error: array b not present on device after create"
        stop 2
    end if

    ! Perform computation on device
    !$acc parallel loop present(a, b)
    do i = 1, n
        b(i) = a(i) * 2.0_dp
    end do
    !$acc end parallel loop

    ! Test copyout: retrieve results from device
    !$acc exit data copyout(b)

    ! Verify b is no longer present
    if (acc_is_present(b)) then
        print *, "Error: array b still present after exit data"
        stop 3
    end if

    ! Clean up a
    !$acc exit data delete(a)

    if (acc_is_present(a)) then
        print *, "Error: array a still present after delete"
        stop 4
    end if

    ! Validate results
    errors = 0
    do i = 1, n
        if (abs(b(i) - real(i, dp) * 2.0_dp) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Validation failed:", errors, "errors"
        stop 5
    end if

end program metal_data_transfer
