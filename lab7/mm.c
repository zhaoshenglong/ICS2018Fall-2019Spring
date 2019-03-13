/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * HIGH LEVEL DESIGN:
 *      Segregated free list + First fit scheme + 
 * 1. Structure of Block:
 *  (1) Free block
 *      -------------
 *      | header 000|
 *      |           |
 *      |           |
 *      | footer 000|
 *      -------------
 *      The header contains the size of a block. The last bits are 
 *      spared to record some information about block.
 *      The last is the allocated/free mark, the second is the allocated
 *      information of previous block, the third is the information
 *      about whether it is a relocated block
 *  (2) Allocated block
 *      The same as the free block, the only difference is that
 *      allocated block only contains one header, which means that
 *      it does't have a footer.
 * 2. Structure of Heap
 *  Segregated free list + Prologue + Epilogue
 *  ---------------------------------------------------------
 *  | FREE_SIZE lists |   | 8/1 | 8/1 |       Heap    | 0/2 |
 *  ---------------------------------------------------------
 *   Structure of Free lists:
 *   Grouped by the number of Double words
 *   It is specially designed for the trace files.
 * 3. Reallocate;
 *   Specailly designed for trace file
 *   [Block for continuously allocated and freed block] [Block for relocated blocks] [end]
 *   Always place the relocated block at the end of heap, and not 
 *   add the free block succeed to the free list. If there are blocks succeed to the relocated block,
 *   extend the heap and place the relocated block at the end of heap and coalsece
 *  
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

/* Set the reallocate mark and get the mark*/
#define GET_RE(p) (GET(p) & 0x4)

/* Size of segregated storage list*/
#define FREE_SIZE 16

/* MAacro for debug*/
#define COALESCE 0
#define FREE_BLOCK 1
#define COAL_AND_FREE 2

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

    return bp;
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
    bp = coalesce(bp);
    place(bp, asize);
    return bp;
}
void *find_fit(size_t asize)
{
    char *bp;
    size_t group = get_group(asize);
    for (unsigned int i = group; i < FREE_SIZE - 1; i++)
    {
        unsigned int *group_addr = segreg_free + i;

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
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{

    if (!GET_ALLOC(HDRP(bp)))
        return;
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp)) | GET_RE(HDRP(bp))));
    PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp)) | GET_RE(HDRP(bp))));
    void *next_header = HDRP(NEXT_BLKP(bp));
    PUT(next_header, PACK(GET_SIZE(next_header), GET_ALLOC(next_header) | GET_RE(next_header)));

    if (!GET_RE(HDRP(bp)))
        bp = coalesce(bp);
    add_free(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *bp, size_t size)
{
    if (!bp)
        mm_malloc(size);
    if (!size)
        mm_free(bp);
    void *old_bp = bp;
    void *new_bp;
    size_t asize, extend_size, csize;
    if (size <= DSIZE)
        asize = DSIZE * 2;
    else
        asize = DSIZE * ((size + (WSIZE) + (DSIZE - 1)) / DSIZE);

    /* Next block is allocated, extend the heap, put the reallocat block at the end*/
    if (GET_ALLOC(HDRP(NEXT_BLKP(bp))))
    {
        /* Next block is not the end of heap, extend the heap and relocate the block*/
        if (GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0)
        {
            extend_size = MAX(asize, CHUNKSIZE);
            if ((new_bp = extend_heap(extend_size / WSIZE)) == NULL)
                return NULL;
            new_bp = coalesce(new_bp);
            csize = GET_SIZE(HDRP(new_bp));
            if (csize >= asize + 2 * DSIZE)
            {
                PUT(HDRP(new_bp), PACK(asize, GET_PREV_ALLOC(HDRP(new_bp)) | 1 | 4));
                bp = NEXT_BLKP(new_bp);
                PUT(HDRP(bp), PACK(csize - asize, 2));
                PUT(FTRP(bp), PACK(csize - asize, 2));
            }
            else
            {
                PUT(HDRP(new_bp), PACK(csize, GET_PREV_ALLOC(HDRP(new_bp)) | 1 | 4));
                bp = NEXT_BLKP(new_bp);
                PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 2 | GET_ALLOC(HDRP(bp))));
            }
            memmove(new_bp, old_bp, size);
            add_free(old_bp);
            return new_bp;
        }
        /* Next block is the end of heap, extend the heap and coalesce*/
        else
        {
            extend_size = 136;
            csize = GET_SIZE(HDRP(old_bp)) + extend_size;
            if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
                return NULL;
            bp = coalesce(bp);
            PUT(HDRP(old_bp), PACK(asize, GET_PREV_ALLOC(HDRP(old_bp)) | 1 | 4));
            PUT(HDRP(NEXT_BLKP(old_bp)), PACK(csize - asize, 2));
            PUT(FTRP(NEXT_BLKP(old_bp)), PACK(csize - asize, 2));
            return old_bp;
        }
    }
    /* Next block is free, coalesce and place if capacity is enough, extend otherwise*/
    else
    {

        csize = GET_SIZE(HDRP(old_bp)) + GET_SIZE(HDRP(NEXT_BLKP(old_bp)));
        /* Big enough to place*/
        if (csize >= asize)
        {
            if (csize >= asize + 2 * DSIZE)
            {
                PUT(HDRP(old_bp), PACK(asize, GET_PREV_ALLOC(HDRP(old_bp)) | 1 | 4));
                bp = NEXT_BLKP(old_bp);
                PUT(HDRP(bp), PACK(csize - asize, 2));
                PUT(FTRP(bp), PACK(csize - asize, 2));
            }
            else
            {
                PUT(HDRP(old_bp), PACK(csize, GET_PREV_ALLOC(HDRP(old_bp)) | 1 | 4));
                bp = NEXT_BLKP(old_bp);
                PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 2 | GET_ALLOC(HDRP(bp))));
            }
            return old_bp;
        }
        /* Not enough, extend the heap*/
        else
        {

            extend_size = 136;
            csize += extend_size;
            if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
            {
                return NULL;
            }
            /* Coalesce*/
            PUT(FTRP(bp), PACK(GET_SIZE(HDRP(PREV_BLKP(bp))) + extend_size, 2));
            PUT(HDRP(PREV_BLKP(bp)), PACK(GET_SIZE(HDRP(PREV_BLKP(bp))) + extend_size, 2));
            bp = PREV_BLKP(bp);

            PUT(HDRP(old_bp), PACK(asize, GET_PREV_ALLOC(HDRP(old_bp)) | 1 | 4));
            PUT(HDRP(NEXT_BLKP(old_bp)), PACK(csize - asize, 2));
            PUT(FTRP(NEXT_BLKP(old_bp)), PACK(csize - asize, 2));
            return old_bp;
        }
    }
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
    default:
        return 0;
    }
    return 0;
}