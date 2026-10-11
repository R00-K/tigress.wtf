#include <stdio.h>

int calculate(int a, int b)
{
    return a + b;
}

int main(void)
{
    printf("Result: %d\n", calculate(10, 20));
    return 0;
}
