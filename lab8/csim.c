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
#include <stddef.h>

#define ADDR_BITS 64

/* Judge the character */
#define IS_LOAD(c) ((c) == 'L')
#define IS_STORE(c) ((c) == 'S')
#define IS_MODIFY(c) ((c) == 'M')
#define IS_INSTR(c) ((c) == 'I')
#define IS_BLANK(c) ((c) == ' ' || (c) == '\t')
#define IS_NEWLINE(c) ((c) == '\n')
#define IS_END(c) ((c) == '\0')
#define SKIP_BLANK(c)                           \
	do                                          \
	{                                           \
		while (IS_BLANK(*(c)) && !IS_END(*(c))) \
			(c)++;                              \
	} while (0);

#define SKIP_LINE(c)                               \
	do                                             \
	{                                              \
		while (!IS_END(*(c)) && !IS_NEWLINE(*(c))) \
			(c)++;                                 \
	} while (0);

/* Cache simulator */
typedef struct line
{
	unsigned long tag;
	struct line *next;
	struct line *prev;
} line_t;

typedef struct set
{
	int linesize;
	int linenum;
	int full;
	line_t *begin;
	line_t *end;
} set_t;

/* Init the cache and free the space in the end */
set_t *init_cache(int set_n, int line_n);
void fr_cache(set_t *cache, int set_n);

/* Load, store, modify operation */
int load(set_t *set, unsigned long tag, int *hit_n, int *miss_n, int *eviction_n, int verbose);
int store(set_t *set, unsigned long tag, int *hit_n, int *miss_n, int *eviction_n, int verbose);
int modify(set_t *set, unsigned long tag, int *hit_n, int *miss_n, int *eviction_n, int verbose);
void printline(char *str);
void parse_addr(unsigned long addr, unsigned long *tag, int *set_idx, int set_bits, int block_bits);
void move_front(line_t *line, line_t *begin);
/* extern variable of getopt in main */
extern char *optarg;

int main(int argc, char *argv[])
{
	/* Number of sets, lines, blocks, misses, hit, evictions */
	int set_n = 1, line_n = 1;
	int miss_n = 0, hit_n = 0, eviction_n = 0;
	int set_bits = 0, block_bits = 0;
	char verbose = 0; /* Sign for verbose or silent */

	unsigned long tag = 0;
	int set_idx = 0;
	FILE *fp;
	char *acc_buf;
	int bufsz = 0;
	int opt;

	/* Get options from stdin */
	while ((opt = getopt(argc, argv, "vs:E:b:t:")) != -1)
	{
		switch (opt)
		{
		case 'v':
			verbose = 1;
			break;
		case 's':
			set_bits = atoi(optarg);
			set_n = 1 << set_bits;
			break;
		case 'E':
			line_n = atoi(optarg);
			break;
		case 'b':
			block_bits = atoi(optarg);
			break;
		case 't':
			fp = fopen((char *)optarg, "r");
			if (!fp)
			{
				printf("ERROR! can not open file %s", optarg);
				exit(1);
			}
			fseek(fp, 0, SEEK_END);
			size_t fp_sz = ftell(fp);
			rewind(fp);
			acc_buf = (char *)malloc(sizeof(char) * fp_sz);
			if (!acc_buf)
			{
				printf("ERROR! malloc failed!");
				exit(2);
			}
			if (fp_sz != fread(acc_buf, 1, fp_sz, fp))
			{
				printf("ERROR! reading block failed!");
				exit(3);
			}
			bufsz = fp_sz;
			break;
		default:
			printf("Unknown option: %c\n", opt);
		}
	}

	/* Start read file and load, store, modify */
	set_t *cache = init_cache(set_n, line_n);

	char *c = acc_buf;

	unsigned long address; /* Physical address to be accessed */

	char *buf_end = acc_buf + bufsz;
	while (c != buf_end)
	{
		SKIP_BLANK(c);
		if (IS_INSTR(*c))
		{
			SKIP_LINE(c);
			c++; /* Seek to next line */
			continue;
		}
		else if (IS_LOAD(*c))
		{
			if (verbose)
				printline(c);
			c++;
			SKIP_BLANK(c);
			address = strtoul(c, NULL, 16);
			parse_addr(address, &tag, &set_idx, set_bits, block_bits);
			load(&cache[set_idx], tag, &hit_n, &miss_n, &eviction_n, verbose);
			SKIP_LINE(c);
			c++;
			continue;
		}
		else if (IS_STORE(*c))
		{
			if (verbose)
				printline(c);
			c++;
			SKIP_BLANK(c);
			address = strtoul(c, NULL, 16);
			parse_addr(address, &tag, &set_idx, set_bits, block_bits);
			store(&cache[set_idx], tag, &hit_n, &miss_n, &eviction_n, verbose);
			SKIP_LINE(c);
			c++;
			continue;
		}
		else if (IS_MODIFY(*c))
		{
			if (verbose)
				printline(c);
			c++;
			SKIP_BLANK(c);
			address = strtoul(c, NULL, 16);
			parse_addr(address, &tag, &set_idx, set_bits, block_bits);
			modify(&cache[set_idx], tag, &hit_n, &miss_n, &eviction_n, verbose);
			SKIP_LINE(c);
			c++;
			continue;
		}
		else
		{
			printf("Wrong! %c\n", *c);
			break;
		}
	}
	/* Free space and close file */
	fclose(fp);
	fr_cache(cache, set_n);
	printSummary(hit_n, miss_n, eviction_n);
	return 0;
}

/* 
 *	Init the cache structure
 */
set_t *init_cache(int set_n, int line_n)
{
	set_t *cache = (set_t *)malloc(sizeof(set_t) * set_n);
	for (int i = 0; i < set_n; i++)
	{
		cache[i].linesize = line_n;
		cache[i].linenum = 0;
		cache[i].full = 0;
		cache[i].end = (line_t *)malloc(sizeof(line_t));
		cache[i].begin = (line_t *)malloc(sizeof(line_t));
		cache[i].begin->prev = NULL;
		cache[i].begin->next = cache[i].end;
		cache[i].end->prev = cache[i].begin;
		cache[i].end->next = NULL;
	}
	return cache;
}

/*
 *	Free the cache  
 */
void fr_cache(set_t *cache, int set_n)
{
	for (int i = 0; i < set_n; i++)
	{
		line_t *line = cache[i].begin->next, *tmp;
		while (line != cache[i].end)
		{
			tmp = line->next;
			free(line);
			line = tmp;
		}
		free(cache[i].begin);
		free(cache[i].end);
	}
	free(cache);
}

/*
 *	Load the block.
	Only check if it is in the cache, not really load
 */
int load(set_t *set, unsigned long tag, int *hit_n, int *miss_n, int *eviction_n, int verbose)
{
	for (line_t *line = set->begin->next; line != set->end; line = line->next)
	{
		if (line->tag == tag)
		{
			(*hit_n)++;
			if (verbose)
				printf(" hit\n");
			/* Move to the end */
			move_front(line, set->begin);
			return 1;
		}
	}
	(*miss_n)++;
	if (set->full)
	{
		(*eviction_n)++;
		line_t *line = set->end->prev;
		line->tag = tag;
		move_front(line, set->begin);
		if (verbose)
		{
			printf(" miss eviction\n");
		}
		return 2;
	}
	else
	{
		line_t *line = (line_t *)malloc(sizeof(line_t));
		line->next = set->begin->next;
		line->prev = set->begin;
		line->next->prev = line;
		set->begin->next = line;
		line->tag = tag;
		set->linenum++;
		if (set->linenum == set->linesize)
			set->full = 1;
		if (verbose)
		{
			printf(" miss\n");
		}
		return 3;
	}
}

int store(set_t *set, unsigned long tag, int *hit_n, int *miss_n, int *eviction_n, int verbose)
{
	return load(set, tag, hit_n, miss_n, eviction_n, verbose);
}

int modify(set_t *set, unsigned long tag, int *hit_n, int *miss_n, int *eviction_n, int verbose)
{
	int load_sig = load(set, tag, hit_n, miss_n, eviction_n, verbose);
	int store_sig = load(set, tag, hit_n, miss_n, eviction_n, verbose);
	return (load_sig << 2) | store_sig;
}

void printline(char *str)
{
	while ((*str) != '\n')
	{
		putchar(*str);
		str++;
	}
}

void parse_addr(unsigned long addr, unsigned long *tag, int *set_idx, int set_bits, int block_bits)
{
	int tag_bits = ADDR_BITS - set_bits - block_bits;
	unsigned long tag_mask, set_mask;
	set_mask = (1 << set_bits) - 1;
	tag_mask = (1 << tag_bits) - 1;
	(*set_idx) = (addr >> block_bits) & set_mask;
	(*tag) = (addr >> (block_bits + set_bits)) & tag_mask;
}

void move_front(line_t *line, line_t *begin)
{
	/* Remove the line first */
	line->prev->next = line->next;
	line->next->prev = line->prev;

	/* Insert into front after then */
	line->next = begin->next;
	line->prev = begin;
	line->next->prev = line;
	begin->next = line;
}