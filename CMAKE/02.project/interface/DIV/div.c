#include<stdio.h>

int div(int p1, int p2)
{
	if (p2 == 0) {
		printf("error input p2\n");
		return 0;
	}

	return (p1 / p2);
}

