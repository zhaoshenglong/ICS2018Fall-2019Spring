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

/* Align the address of blocks to be loaded */
#define ALIGN(p) ((((p) + block_n - 1) / block_n) * block_n)

#define ADDR_NUM 64

/* Judge the character */
#define IS_LOAD(c) ((c) == 'L')
#define IS_STORE(c) ((c) == 'S')
#define IS_MODIFY(c) ((c) == 'M')
#define IS_INSTR(c) ((c) == 'I')
#define IS_BLANK(c) ((c) == ' ' || (c) == '\t')
#define IS_LINE_END ((c) == '\n')

/* Cache simulator */
typedef struct line {
	char valid;
	long tag;
	long LRU;
}line_t;

typedef struct set {
	int linenum;
	line_t *line;
}set_t;

/* Init the cache and free the space in the end */
set_t *init_cache();
void fr_cache(set_t *cache);


/* Load, store, modify operation */
int load ();
int store();
int modify();


/* extern variable of getopt in main */
extern char *optarg;

int main(int argc, char *argv[])
{
   	/* Number of sets, lines, blocks, misses, hit, evictions */
	int set_n = 1, line_n = 1, block_n = 1;
	int miss_n = 0, hit_n = 0, eviction_n = 0;
	
	char verbose = 0; 									/* Sign for verbose or silent */

		
	FILE *fp;
	char *acc_buf;
	int bufsz = 0;
	int opt;

	/* Get options from stdin */
	while((opt = getopt(argc, argv, "vs:E:b:t:")) != -1){
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
			fp = fopen((char *)optarg, "r");
			if( !fp ){
				printf("ERROR! can not open file %s", optarg);
				exit(1);
			}
			fseek (fp, 0, SEEK_END);
			size_t fp_sz = ftell(fp);
			rewind(fp);
			acc_buf = (char *)malloc(sizeof(char) * fp_sz);
			if (!acc_buf) {
				printf("ERROR! malloc failed!");
				exit(2);
			}
			if (fp_sz != fread(acc_buf, 1, fp_sz, fp)){
				printf("ERROR! reading block failed!");
				exit(3);
			}
			bufsz = fp_sz;
			break;
		default:
			printf("unknown character: %c\n", opt);
		}
	}
	
	/* Start read file and load, store, modify */
//	set_t *cache = init_cache(set_n, line_n);
	int tag_n = ADDR_NUM - set_n - block_n;
	int c;
	
	
	size_t address;										/* Physical address to be accessed */
	
	printf(acc_buf);

	/* Free space and close file */
	fclose(fp);
	//fr_cache(cache);
//	printSummary(hit_n, miss_n, eviction_n);
    return 0;
}
