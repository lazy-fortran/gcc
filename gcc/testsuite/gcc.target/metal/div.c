/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_div_int(int a, int b)
{
    return a / b;
}

unsigned int test_div_uint(unsigned int a, unsigned int b)
{
    return a / b;
}

long test_div_long(long a, long b)
{
    return a / b;
}

float test_div_float(float a, float b)
{
    return a / b;
}

double test_div_double(double a, double b)
{
    return a / b;
}
