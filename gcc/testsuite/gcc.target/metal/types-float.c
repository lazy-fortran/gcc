/* { dg-do compile } */
/* { dg-options "-O2" } */

float test_float_ops(float a, float b)
{
    float sum = a + b;
    float diff = a - b;
    float prod = a * b;
    float quot = a / b;
    return sum + diff + prod + quot;
}

double test_double_ops(double a, double b)
{
    double sum = a + b;
    double diff = a - b;
    double prod = a * b;
    double quot = a / b;
    return sum + diff + prod + quot;
}

long double test_long_double_ops(long double a, long double b)
{
    long double sum = a + b;
    long double diff = a - b;
    long double prod = a * b;
    long double quot = a / b;
    return sum + diff + prod + quot;
}

float test_float_conversions(int i, double d)
{
    float f1 = (float)i;
    float f2 = (float)d;
    return f1 + f2;
}

int test_float_to_int(float f, double d)
{
    int i1 = (int)f;
    int i2 = (int)d;
    return i1 + i2;
}
