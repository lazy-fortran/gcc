/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_and(int a, int b)
{
    return a & b;
}

int test_or(int a, int b)
{
    return a | b;
}

int test_xor(int a, int b)
{
    return a ^ b;
}

int test_not(int a)
{
    return ~a;
}

int test_shl(int a, int b)
{
    return a << b;
}

int test_shr(int a, int b)
{
    return a >> b;
}
