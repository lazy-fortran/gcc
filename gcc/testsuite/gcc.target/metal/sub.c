/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_sub_int(int a, int b)
{
    return a - b;
}

unsigned int test_sub_uint(unsigned int a, unsigned int b)
{
    return a - b;
}

long test_sub_long(long a, long b)
{
    return a - b;
}

float test_sub_float(float a, float b)
{
    return a - b;
}

double test_sub_double(double a, double b)
{
    return a - b;
}
