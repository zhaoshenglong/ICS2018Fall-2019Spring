/**
 * INFORMATION For Lab8:
 * 	Student name: zhaoshenglong
 * 	Student ID  : 515030910241
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
/* Align the address of blocks to be loaded*/
#define ALIGN(p) ((((p) + block_n - 1) / block_n) * block_n)

/* Cache type*/



extern char *optarg;
int main(int argc, char *argv[])
{
   	/* Number of sets, lines, blocks, misses, hit, evictions*/
	int set_n = 1, line_n = 1, block_n = 1;	
	int miss_n = 0, hit_n = 0, eviction_n = 0;
	
	/* Sign for verbose or silent*/
	char verbose = 0;
	
	int opt;
	/* Get options from stdin*/
	while((opt = getopt(argc, argv, "-vs:E:b:t:")) != -1){
		switch(opt){
		case 'v':
			verbose = 1;
			break;	
		case 's':
			set_n = atoi(optarg);
			break;
		case 'E':
			line_n = atoi(optarg);
			break;
		case 'b':
			block_n = atoi(optarg);
			break;
		case 't':
			break;
		default:
			printf("unknown character: %c\n", opt);
		}
	}
	
	printSummary(hit_n, miss_n, eviction_n);
    return 0;
}
