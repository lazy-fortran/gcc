! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_accel_selected } }
! { dg-additional-options "-fopenacc" }

! Test combined Metal features: async + reduction + data transfer

program metal_combined
    use openacc
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 2048
    real(dp) :: a(n), b(n), c(n)
    real(dp) :: sum1, sum2, sum3
    integer :: i, errors

    ! Initialize
    do i = 1, n
        a(i) = real(i, dp)
        b(i) = real(i * 2, dp)
        c(i) = 0.0_dp
    end do

    ! Test 1: Async operations with reductions
    sum1 = 0.0_dp
    sum2 = 0.0_dp

    !$acc parallel loop async(1) reduction(+:sum1)
    do i = 1, n
        sum1 = sum1 + a(i)
    end do
    !$acc end parallel loop

    !$acc parallel loop async(2) reduction(+:sum2)
    do i = 1, n
        sum2 = sum2 + b(i)
    end do
    !$acc end parallel loop

    !$acc wait(1)
    !$acc wait(2)

    if (abs(sum1 - real(n, dp) * real(n + 1, dp) / 2.0_dp) > 1.0e-6_dp) then
        print *, "Test 1 failed: sum1 incorrect"
        stop 1
    end if

    if (abs(sum2 - real(n, dp) * real(n + 1, dp)) > 1.0e-6_dp) then
        print *, "Test 1 failed: sum2 incorrect"
        stop 2
    end if

    ! Test 2: Data regions with async and reductions
    sum1 = 0.0_dp
    sum2 = 0.0_dp
    sum3 = 0.0_dp

    !$acc data copyin(a, b) copyout(c)

    !$acc parallel loop async(1)
    do i = 1, n
        c(i) = a(i) + b(i)
    end do
    !$acc end parallel loop

    !$acc parallel loop async(1) reduction(+:sum1)
    do i = 1, n
        sum1 = sum1 + a(i)
    end do
    !$acc end parallel loop

    !$acc parallel loop async(2) reduction(+:sum2)
    do i = 1, n
        sum2 = sum2 + b(i)
    end do
    !$acc end parallel loop

    !$acc parallel loop wait(1) async(3) reduction(+:sum3)
    do i = 1, n
        sum3 = sum3 + c(i)
    end do
    !$acc end parallel loop

    !$acc wait

    !$acc end data

    if (abs(sum3 - (sum1 + sum2)) > 1.0e-6_dp) then
        print *, "Test 2 failed: reduction mismatch"
        stop 3
    end if

    errors = 0
    do i = 1, n
        if (abs(c(i) - (a(i) + b(i))) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 2 failed:", errors, "array errors"
        stop 4
    end if

    ! Test 3: Kernels with async and data
    c(:) = 0.0_dp
    sum1 = 0.0_dp

    !$acc data copyin(a, b) copyout(c)

    !$acc kernels async(1)
    do i = 1, n
        c(i) = a(i) * b(i)
    end do
    !$acc end kernels

    !$acc parallel loop async(2) reduction(+:sum1)
    do i = 1, n
        sum1 = sum1 + a(i)
    end do
    !$acc end parallel loop

    !$acc wait

    !$acc end data

    errors = 0
    do i = 1, n
        if (abs(c(i) - (a(i) * b(i))) > 1.0e-10_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 3 failed:", errors, "array errors"
        stop 5
    end if

    ! Test 4: Complex async dependency with multiple operations
    a(:) = 1.0_dp
    b(:) = 2.0_dp
    c(:) = 0.0_dp

    !$acc enter data copyin(a, b) create(c)

    !$acc parallel loop async(1)
    do i = 1, n
        a(i) = a(i) + 10.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop async(2)
    do i = 1, n
        b(i) = b(i) + 20.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop wait(1) wait(2) async(3)
    do i = 1, n
        c(i) = a(i) + b(i)
    end do
    !$acc end parallel loop

    !$acc wait(3)

    !$acc exit data copyout(a, b, c)

    errors = 0
    do i = 1, n
        if (abs(a(i) - 11.0_dp) > 1.0e-12_dp) errors = errors + 1
        if (abs(b(i) - 22.0_dp) > 1.0e-12_dp) errors = errors + 1
        if (abs(c(i) - 33.0_dp) > 1.0e-12_dp) errors = errors + 1
    end do

    if (errors > 0) then
        print *, "Test 4 failed:", errors, "array errors"
        stop 6
    end if

end program metal_combined
