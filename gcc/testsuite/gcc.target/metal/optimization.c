/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_constant_fold(void)
{
    return 2 + 3 * 4;
}

int test_dead_code(int x)
{
    int unused = 42;
    return x + 1;
}

int test_common_subexpr(int a, int b)
{
    int x = a + b;
    int y = a + b;
    return x + y;
}

int test_loop_unroll(void)
{
    int sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += i;
    }
    return sum;
}

int test_strength_reduction(int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += i * 8;
    }
    return sum;
}

int test_branch_elimination(int x)
{
    if (1) {
        return x;
    } else {
        return 0;
    }
}
