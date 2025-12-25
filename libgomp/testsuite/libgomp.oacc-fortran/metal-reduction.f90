! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_accel_selected } }
! { dg-additional-options "-fopenacc" }

! Test Metal reduction operations

program metal_reduction
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 4096
    real(dp) :: array(n)
    real(dp) :: sum_result, product_result, max_result, min_result
    real(dp) :: expected_sum, expected_product, expected_max, expected_min
    integer :: i, int_sum, int_product, int_max, int_min
    integer :: int_array(n)

    ! Initialize test data
    do i = 1, n
        array(i) = real(i, dp)
        int_array(i) = i
    end do

    ! Test 1: Sum reduction
    sum_result = 0.0_dp

    !$acc parallel loop reduction(+:sum_result)
    do i = 1, n
        sum_result = sum_result + array(i)
    end do
    !$acc end parallel loop

    expected_sum = real(n, dp) * real(n + 1, dp) / 2.0_dp
    if (abs(sum_result - expected_sum) > 1.0e-6_dp) then
        print *, "Test 1 (sum reduction) failed"
        print *, "Expected:", expected_sum, "Got:", sum_result
        stop 1
    end if

    ! Test 2: Product reduction (limited range to avoid overflow)
    product_result = 1.0_dp

    !$acc parallel loop reduction(*:product_result)
    do i = 1, 10
        product_result = product_result * real(i, dp)
    end do
    !$acc end parallel loop

    expected_product = 3628800.0_dp
    if (abs(product_result - expected_product) > 1.0e-6_dp) then
        print *, "Test 2 (product reduction) failed"
        print *, "Expected:", expected_product, "Got:", product_result
        stop 2
    end if

    ! Test 3: Max reduction
    max_result = -1.0e30_dp

    !$acc parallel loop reduction(max:max_result)
    do i = 1, n
        max_result = max(max_result, array(i))
    end do
    !$acc end parallel loop

    expected_max = real(n, dp)
    if (abs(max_result - expected_max) > 1.0e-12_dp) then
        print *, "Test 3 (max reduction) failed"
        print *, "Expected:", expected_max, "Got:", max_result
        stop 3
    end if

    ! Test 4: Min reduction
    min_result = 1.0e30_dp

    !$acc parallel loop reduction(min:min_result)
    do i = 1, n
        min_result = min(min_result, array(i))
    end do
    !$acc end parallel loop

    expected_min = 1.0_dp
    if (abs(min_result - expected_min) > 1.0e-12_dp) then
        print *, "Test 4 (min reduction) failed"
        print *, "Expected:", expected_min, "Got:", min_result
        stop 4
    end if

    ! Test 5: Integer sum reduction
    int_sum = 0

    !$acc parallel loop reduction(+:int_sum)
    do i = 1, n
        int_sum = int_sum + int_array(i)
    end do
    !$acc end parallel loop

    if (int_sum /= (n * (n + 1)) / 2) then
        print *, "Test 5 (integer sum) failed"
        print *, "Expected:", (n * (n + 1)) / 2, "Got:", int_sum
        stop 5
    end if

    ! Test 6: Integer max reduction
    int_max = -1000000

    !$acc parallel loop reduction(max:int_max)
    do i = 1, n
        int_max = max(int_max, int_array(i))
    end do
    !$acc end parallel loop

    if (int_max /= n) then
        print *, "Test 6 (integer max) failed"
        print *, "Expected:", n, "Got:", int_max
        stop 6
    end if

    ! Test 7: Integer min reduction
    int_min = 1000000

    !$acc parallel loop reduction(min:int_min)
    do i = 1, n
        int_min = min(int_min, int_array(i))
    end do
    !$acc end parallel loop

    if (int_min /= 1) then
        print *, "Test 7 (integer min) failed"
        print *, "Expected: 1 Got:", int_min
        stop 7
    end if

    ! Test 8: Multiple reductions in same construct
    sum_result = 0.0_dp
    max_result = -1.0e30_dp
    min_result = 1.0e30_dp

    !$acc parallel loop reduction(+:sum_result) &
    !$acc              reduction(max:max_result) &
    !$acc              reduction(min:min_result)
    do i = 1, n
        sum_result = sum_result + array(i)
        max_result = max(max_result, array(i))
        min_result = min(min_result, array(i))
    end do
    !$acc end parallel loop

    expected_sum = real(n, dp) * real(n + 1, dp) / 2.0_dp
    if (abs(sum_result - expected_sum) > 1.0e-6_dp .or. &
        abs(max_result - real(n, dp)) > 1.0e-12_dp .or. &
        abs(min_result - 1.0_dp) > 1.0e-12_dp) then
        print *, "Test 8 (multiple reductions) failed"
        stop 8
    end if

    ! Test 9: Reduction with gang, worker, vector
    sum_result = 0.0_dp

    !$acc parallel loop gang worker vector reduction(+:sum_result)
    do i = 1, n
        sum_result = sum_result + array(i)
    end do
    !$acc end parallel loop

    expected_sum = real(n, dp) * real(n + 1, dp) / 2.0_dp
    if (abs(sum_result - expected_sum) > 1.0e-6_dp) then
        print *, "Test 9 (gang worker vector reduction) failed"
        print *, "Expected:", expected_sum, "Got:", sum_result
        stop 9
    end if

    ! Test 10: Reduction with kernels construct
    sum_result = 0.0_dp

    !$acc kernels
    do i = 1, n
        sum_result = sum_result + array(i)
    end do
    !$acc end kernels

    expected_sum = real(n, dp) * real(n + 1, dp) / 2.0_dp
    if (abs(sum_result - expected_sum) > 1.0e-6_dp) then
        print *, "Test 10 (kernels reduction) failed"
        print *, "Expected:", expected_sum, "Got:", sum_result
        stop 10
    end if

end program metal_reduction
