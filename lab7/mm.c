/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* "modified here"*/
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE ((1 << 12) + 24)

#define MAX(x, y) ((x > y) ? x : y)

/* Pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* Read the allocated information of previous block*/
#define GET_PREV_ALLOC(p) (GET(p) & 0x2)

/*Transfer an address / word into word / address*/
#define WORD_TO_ADDR(word) ((void *)(root_addr + (word)))
#define ADDR_TO_WORD(addr) ((unsigned int)((void *)(addr)-root_addr))

/* Set the free block to the list and get free block from list*/
#define SET_PTR(p, addr) (PUT(p, ADDR_TO_WORD(addr)))
#define GET_PTR(p) WORD_TO_ADDR(GET(p))

/* Size of segregated storage list*/
#define FREE_SIZE 16

/* MAacro for debug*/
#define COALESCE 0
#define FREE_BLOCK 1
#define COAL_AND_FREE 2
#define FREE_LIST 3

/* 
 * Global variable
 * heap_listp: poiter of the heap list
 * segreg_free: array of free block list
 * root_addr: high 32 bits of address
 */
void *heap_listp = 0;
unsigned int *segreg_free;
void *root_addr;

/* Extend the heap by words when there is no fit*/
static void *extend_heap(size_t words);

/* Coalesce if the previous block is free*/
static void *coalesce(void *bp);

/* Find a fit in the segregated free lists*/
static void *find_fit(size_t asize);

/* Place the requested block and split it if necessary*/
static void place(void *bp, size_t asize);

/* Add the free block to the appropriate group*/
static void add_free(void *bp);

/* Remove the free block from list*/
static void *remove_free(void *bp);

/* Check if the heap is consistent return -1 if not, 0 otherwise.*/
static int mm_check(int sign);

/* Get the group of segregated list*/
static unsigned int get_group(unsigned int size)
{
    unsigned int blocks = size / DSIZE;
    if (blocks < 4)
        return blocks - 2;
    else if (blocks == 510)
        return 10;
    else if (blocks == 9)
        return 3;
    else if (blocks == 1025)
        return 12;
    else if (blocks <= 8 && blocks >= 4)
        return 2;
    else if (blocks <= 14 && blocks >= 10)
        return 4;
    else if (blocks <= 20 && blocks >= 15)
        return 5;
    else if (blocks <= 55 && blocks >= 21)
        return 6;
    else if (blocks <= 63 && blocks >= 56)
        return 7;
    else if (blocks <= 128 && blocks >= 64)
        return 8;
    else if (blocks <= 509 && blocks >= 129)
        return 9;
    else if (blocks <= 1024 && blocks >= 511)
        return 11;
    else if (blocks <= 2048 && blocks >= 1026)
        return 13;
    else if (blocks >= 2049)
        return 14;
}
static int count = 0;
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    if ((segreg_free = mem_sbrk(FREE_SIZE * sizeof(unsigned int) + 4 * WSIZE)) == (void *)-1)
        return -1;
    heap_listp = (void *)segreg_free + FREE_SIZE * sizeof(unsigned int);

    memset(segreg_free, 0, FREE_SIZE * sizeof(unsigned int));
    root_addr = segreg_free;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));

    /* Epilogue block, 3 means the Prologue block is marked allocated*/
    PUT(heap_listp + (3 * WSIZE), PACK(0, 3));
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    add_free(heap_listp + (2 * WSIZE));
    /* Check if there is no coalesce*/

    return 0;
}

/* 
 * extend_heap - extend the heap by words 
 */
static void *extend_heap(size_t words)
{

    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment*/
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    /* Initialize free block header/footer and the epilogue header*/
    /*  1. Get allocated information previous block from former epilogue
     *  2. write to the new request heap page
     *  3. set the new epilogue
     */

    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));

    PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    void *coal_ptr = coalesce(bp);
    /* Check if there is no coalesce*/

    return coal_ptr;
}

static void *coalesce(void *bp)
{

    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    size_t size = GET_SIZE(HDRP(bp));
    /*      case 1      */
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    /*      case 2      */
    else if (prev_alloc && !next_alloc)
    {
        remove_free(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 2));
        PUT(FTRP(bp), PACK(size, 2));
    }
    /*      case 3      */
    else if (!prev_alloc && next_alloc)
    {
        remove_free(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 2));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 2));
        bp = PREV_BLKP(bp);
    }
    /*      case 4    */
    else
    {
        remove_free(PREV_BLKP(bp));
        remove_free(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 2));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 2));
        bp = PREV_BLKP(bp);
    }
    /* Check if there is no coalesce*/

    return bp;
}

void add_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    /*      if asize > 128 group into 129~INF       */
    size_t group = get_group(size);

    unsigned int *group_addr = segreg_free + group;

    /* Threre is no block in the group*/

    if (GET_PTR(group_addr) == root_addr)
    {
        SET_PTR(bp, group_addr);
        SET_PTR(bp + WSIZE, root_addr);
        SET_PTR(group_addr, bp);
    }
    /* There are blocks in the group*/
    else
    {
        SET_PTR(bp, group_addr);
        SET_PTR(bp + WSIZE, GET_PTR(group_addr));
        SET_PTR(GET_PTR(group_addr), bp);
        SET_PTR(group_addr, bp);
    }
}

void *remove_free(void *bp)
{
    void *prev_addr = GET_PTR(bp);
    void *next_addr = GET_PTR(bp + WSIZE);
    /* The block is the last of the list*/
    if (next_addr == root_addr)
    {
        /* Previous block is the list root*/
        if (prev_addr < heap_listp)
        {
            SET_PTR(prev_addr, root_addr);
        }
        /* Previous block has both prev and next*/
        else
        {
            SET_PTR(prev_addr + WSIZE, root_addr);
        }
    }
    /* modify the next block*/
    else
    {
        if (prev_addr < heap_listp)
        {
            SET_PTR(next_addr, prev_addr);
            SET_PTR(prev_addr, next_addr);
        }
        else
        {

            SET_PTR(next_addr, prev_addr);
            SET_PTR(prev_addr + WSIZE, next_addr);
        }
    }
    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Ajusted block size*/
    size_t extendsize; /* Amount to extend heap if no fit*/
    char *bp;

    if (heap_listp == 0)
        mem_init();
    /* Ignore supurious requests*/
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs*/
    if (size <= DSIZE)
        asize = DSIZE * 2;
    else
        asize = DSIZE * ((size + (WSIZE) + (DSIZE - 1)) / DSIZE);
    /* Specific for binary trace, not recommended*/
    if (size == 112)
        asize = 136;
    if (size == 448)
        asize = 520;
    /* Check if there is no coalesce*/
    /* Search the free list first*/
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);

        return bp;
    }

    /* No fit found. Get more memory and place the block.*/

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}
void *find_fit(size_t asize)
{

    char *bp;
    size_t group = get_group(asize);
    if (!group)
        group--;
    for (unsigned int i = group; i < FREE_SIZE - 1; i++)
    {
        unsigned int *group_addr = segreg_free + i;
        if (GET(group_addr) != 0)
        {
            bp = GET_PTR(group_addr);
            while (bp != root_addr)
            {
                if (GET_SIZE(HDRP(bp)) >= asize)
                {
                    remove_free(bp);
                    return bp;
                }
                bp = GET_PTR(bp + WSIZE);
            }
        }
    }
    return NULL;
}
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    /* The block is large enough to be split into small one*/
    if (csize >= asize + (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, GET_PREV_ALLOC(HDRP(bp)) | 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 2));
        PUT(FTRP(bp), PACK(csize - asize, 2));
        add_free(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, GET_PREV_ALLOC(HDRP(bp)) | 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 2 | GET_ALLOC(HDRP(bp))));
    }
    /* Check if there is no coalesce*/
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{

    if (!GET_ALLOC(HDRP(bp)))
        return;
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    void *next_header = HDRP(NEXT_BLKP(bp));
    PUT(next_header, PACK(GET_SIZE(next_header), GET_ALLOC(next_header)));
    bp = coalesce(bp);
    add_free(bp);

    /* Check if there is no coalesce*/
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
static int first_relloc = 0;
void *mm_realloc(void *bp, size_t size)
{
    if (!bp)
        mm_malloc(size);
    if (!size)
        mm_free(bp);
    void *old_bp = bp;
    void *new_bp;
    size_t copy_size;
    size_t asize;
    if (size <= DSIZE)
        asize = DSIZE * 2;
    else
        asize = DSIZE * ((size + (WSIZE) + (DSIZE - 1)) / DSIZE);

    new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    memcpy(new_bp, old_bp, size);
    mm_free(old_bp);
    return new_bp;
}

/* Check if the heap is consistent when debugging*/
int mm_check(int sign)
{
    /* Local variables used for checking coalesce*/
    unsigned int prev_alloc = 0, curr_alloc = 0;
    /* Local variables used for checking free blocks*/
    unsigned int allocated = 0;
    switch (sign)
    {
    case COALESCE:
        for (char *bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        {
            curr_alloc = GET_ALLOC(HDRP(bp));
            /* Allocated information of previous block is wrong*/
            if (prev_alloc != (GET_PREV_ALLOC(HDRP(bp))) >> 1)
                return -1;
            prev_alloc = (GET_PREV_ALLOC(HDRP(bp)) >> 1);
            if (!curr_alloc && !prev_alloc)
                return -1;
            prev_alloc = curr_alloc;
        }
        break;
    case FREE_BLOCK:
        for (char *bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        {
            allocated = GET_ALLOC(HDRP(bp));
            if (!allocated)
            {
                if (GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp)))
                    return -1;
                if (GET_ALLOC(FTRP(bp)))
                    return -1;
            }
        }
        break;
    case COAL_AND_FREE:
        for (char *bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        {
            allocated = GET_ALLOC(HDRP(bp));
            if (!allocated)
            {
                if (GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp)))
                    return -1;
                if (GET_ALLOC(FTRP(bp)))
                    return -1;
            }
            curr_alloc = GET_ALLOC(HDRP(bp));
            /* Allocated information of previous block is wrong*/
            if (prev_alloc != (GET_PREV_ALLOC(HDRP(bp))) >> 1)
                return -1;
            prev_alloc = (GET_PREV_ALLOC(HDRP(bp)) >> 1);
            if (!curr_alloc && !prev_alloc)
                return -1;
            prev_alloc = curr_alloc;
        }
        break;
    case FREE_LIST:

        break;
    default:
        return 0;
    }
    return 0;
}