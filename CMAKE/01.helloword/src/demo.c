#include <stdio.h>

void main(void);

int add(int p1, int p2)
{
	return (p1 + p2);
}

int sub(int p1, int p2)
{
	return (p1 - p2);
}

int mul(int p1, int p2)
{
	return (p1 * p2);
}

int div(int p1, int p2)
{
	if (p2 == 0) {
		printf("error input p2\n");
		return 0;
	}

	return (p1 / p2);
}

void main(void)
{
	int a = 100;
	int b = 10;

	printf("This is a test demo\n");
	printf("%d+%d=%d\n", a, b, add(a,b));
	printf("%d-%d=%d\n", a, b, sub(a,b));
	printf("%d*%d=%d\n", a, b, mul(a,b));
	printf("%d/%d=%d\n", a, b, div(a,b));

}

