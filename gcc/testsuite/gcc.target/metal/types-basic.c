/* { dg-do compile } */
/* { dg-options "-O2" } */

char test_char(char a, char b)
{
    return a + b;
}

short test_short(short a, short b)
{
    return a + b;
}

int test_int(int a, int b)
{
    return a + b;
}

long test_long(long a, long b)
{
    return a + b;
}

long long test_long_long(long long a, long long b)
{
    return a + b;
}

unsigned char test_uchar(unsigned char a, unsigned char b)
{
    return a + b;
}

unsigned short test_ushort(unsigned short a, unsigned short b)
{
    return a + b;
}

unsigned int test_uint(unsigned int a, unsigned int b)
{
    return a + b;
}

unsigned long test_ulong(unsigned long a, unsigned long b)
{
    return a + b;
}

unsigned long long test_ullong(unsigned long long a, unsigned long long b)
{
    return a + b;
}
