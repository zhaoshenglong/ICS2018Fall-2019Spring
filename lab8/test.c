#include <stdio.h>
void incre(int *num);
int main()
{
    int integer = 1000;
    int *ip = &integer;
    (*ip)++;
    printf("*ip %d, integer: %d\n", *ip, integer);
    incre(ip);
    printf("*ip %d, integer: %d\n", *ip, integer);
    incre(&integer);
    printf("*ip %d, integer: %d\n", *ip, integer);
    return 0;
}
void incre(int *num)
{
    (*num)++;
}