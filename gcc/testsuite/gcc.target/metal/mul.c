/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_mul_int(int a, int b)
{
    return a * b;
}

unsigned int test_mul_uint(unsigned int a, unsigned int b)
{
    return a * b;
}

long test_mul_long(long a, long b)
{
    return a * b;
}

float test_mul_float(float a, float b)
{
    return a * b;
}

double test_mul_double(double a, double b)
{
    return a * b;
}
