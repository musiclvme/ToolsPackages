#include <stdio.h>
#include "config.h"

#ifdef ENABLE_INTERFACE
#include "add.h"
#include "sub.h"
#include "mul.h"
#include "div.h"
#else
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
#endif

void main(void)
{
	int a = 100;
	int b = 10;
	
	#ifdef ENABLE_INTERFACE
	printf("This is a test demo--->using my own interface\n");
	#else
	printf("This is a test demo\n");
	#endif
	printf("%d+%d=%d\n", a, b, add(a,b));
	printf("%d-%d=%d\n", a, b, sub(a,b));
	printf("%d*%d=%d\n", a, b, mul(a,b));
	printf("%d/%d=%d\n", a, b, div(a,b));

}

