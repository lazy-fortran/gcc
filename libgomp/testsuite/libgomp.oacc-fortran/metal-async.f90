! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_selected } }
! { dg-additional-options "-fopenacc" }

! Test Metal async operations and queue management

program metal_async
    use openacc
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 2048
    real(dp) :: a(n), b(n), c(n), d(n), e(n)
    integer :: i, errors

    ! Initialize arrays
    do i = 1, n
        a(i) = real(i, dp)
        b(i) = 0.0_dp
        c(i) = 0.0_dp
        d(i) = 0.0_dp
        e(i) = 0.0_dp
    end do

    ! Test 1: Single async operation
    !$acc parallel loop async
    do i = 1, n
        b(i) = a(i) * 2.0_dp
    end do
    !$acc end parallel loop

    !$acc wait

    errors = 0
    do i = 1, n
        if (abs(b(i) - real(i * 2, dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 1 (simple async) failed:", errors, "errors"
        stop 1
    end if

    ! Test 2: Multiple async operations with queue IDs
    b(:) = 0.0_dp
    c(:) = 0.0_dp

    !$acc parallel loop async(1)
    do i = 1, n
        b(i) = a(i) + 10.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop async(2)
    do i = 1, n
        c(i) = a(i) + 20.0_dp
    end do
    !$acc end parallel loop

    !$acc wait(1)
    !$acc wait(2)

    errors = 0
    do i = 1, n
        if (abs(b(i) - (real(i, dp) + 10.0_dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
        if (abs(c(i) - (real(i, dp) + 20.0_dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 2 (async queues) failed:", errors, "errors"
        stop 2
    end if

    ! Test 3: Async dependency chain
    b(:) = 0.0_dp
    c(:) = 0.0_dp
    d(:) = 0.0_dp

    !$acc parallel loop async(1)
    do i = 1, n
        b(i) = a(i) * 2.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop async(1)
    do i = 1, n
        c(i) = b(i) + 5.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop async(1)
    do i = 1, n
        d(i) = c(i) * 3.0_dp
    end do
    !$acc end parallel loop

    !$acc wait(1)

    errors = 0
    do i = 1, n
        if (abs(d(i) - ((real(i, dp) * 2.0_dp + 5.0_dp) * 3.0_dp)) > &
            1.0e-10_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 3 (dependency chain) failed:", errors, "errors"
        stop 3
    end if

    ! Test 4: Multiple independent async queues
    b(:) = 0.0_dp
    c(:) = 0.0_dp
    d(:) = 0.0_dp
    e(:) = 0.0_dp

    !$acc parallel loop async(1)
    do i = 1, n
        b(i) = a(i) * 3.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop async(2)
    do i = 1, n
        c(i) = a(i) * 4.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop async(3)
    do i = 1, n
        d(i) = a(i) * 5.0_dp
    end do
    !$acc end parallel loop

    !$acc parallel loop wait(1) wait(2) wait(3) async(4)
    do i = 1, n
        e(i) = b(i) + c(i) + d(i)
    end do
    !$acc end parallel loop

    !$acc wait(4)

    errors = 0
    do i = 1, n
        if (abs(e(i) - (real(i, dp) * 12.0_dp)) > 1.0e-10_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 4 (independent queues) failed:", errors, "errors"
        stop 4
    end if

    ! Test 5: Async with data regions
    b(:) = 0.0_dp

    !$acc data copy(a, b)

    !$acc parallel loop async(1)
    do i = 1, n
        b(i) = a(i) * 7.0_dp
    end do
    !$acc end parallel loop

    !$acc wait(1)

    !$acc end data

    errors = 0
    do i = 1, n
        if (abs(b(i) - (real(i, dp) * 7.0_dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 5 (async with data) failed:", errors, "errors"
        stop 5
    end if

end program metal_async
