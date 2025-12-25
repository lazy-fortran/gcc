/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_array_local(void)
{
    int arr[10];
    arr[0] = 1;
    arr[9] = 10;
    return arr[0] + arr[9];
}

int test_array_init(void)
{
    int arr[5] = {1, 2, 3, 4, 5};
    return arr[0] + arr[4];
}

int test_array_sum(int arr[], int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}

void test_array_fill(int arr[], int n, int value)
{
    for (int i = 0; i < n; i++) {
        arr[i] = value;
    }
}

int test_multidim_array(int rows, int cols)
{
    int arr[10][10];
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            arr[i][j] = i * cols + j;
        }
    }
    return arr[0][0] + arr[rows-1][cols-1];
}
