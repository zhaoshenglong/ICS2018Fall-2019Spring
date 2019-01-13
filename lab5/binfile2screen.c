#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv){
	FILE *in;
	in = fopen(argv[1],"r");
	if (!in )
		return -1;
	int c;
	while((c = fgetc(in)) != EOF){
		printf("%.2x",c);
	}
	printf("\n");
	return 0;
}

