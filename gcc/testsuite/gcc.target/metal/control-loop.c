/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_for_loop(int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += i;
    }
    return sum;
}

int test_while_loop(int n)
{
    int sum = 0;
    int i = 0;
    while (i < n) {
        sum += i;
        i++;
    }
    return sum;
}

int test_do_while_loop(int n)
{
    int sum = 0;
    int i = 0;
    do {
        sum += i;
        i++;
    } while (i < n);
    return sum;
}

int test_nested_loop(int n, int m)
{
    int sum = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            sum += i * j;
        }
    }
    return sum;
}
