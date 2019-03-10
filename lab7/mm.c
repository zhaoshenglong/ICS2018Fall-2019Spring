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
#define CHUNKSIZE (1 << 12)

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
#define WORD_TO_ADDR(word) ((unsigned long)(addr_head | (unsigned long)word))
#define ADDR_TO_WORD(addr) ((unsigned int)addr)

/* Set the free block to the list and get free block from list*/
#define SET_PTR(p, addr) (PUT(p, ADDR_TO_WORD(addr)))
#define GET_PTR(p) WORD_TO_ADDR(GET(p))

/* Size of segregated storage list*/
#define FREE_SIZE 128

/* 
 * Global variable
 * heap_listp: poiter of the heap list
 * segreg_free: array of free block list
 * addr_head: high 32 bits of address
 */
void *heap_listp = 0;
unsigned int *segreg_free;
unsigned long addr_head;

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

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    if ((segreg_free = mem_sbrk(FREE_SIZE * sizeof(unsigned int) + 4 * WSIZE)) == (void *)-1)
        return -1;
    heap_listp = segreg_free + FREE_SIZE * sizeof(unsigned int);
    memset(segreg_free, 0, FREE_SIZE * sizeof(unsigned int));
    addr_head = (long)heap_listp & (((long)1 << 63) >> 31);

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));

    /* Epilogue block, 3 means the Prologue block is marked allocated*/
    PUT(heap_listp + (3 * WSIZE), PACK(0, 3));

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    add_free(heap_listp + (3 * WSIZE));
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

    /* Coalesce if the previous block was free*/
    void *coal_ptr = coalesce(bp);
    /* Is it necessary to add_dree? Since extend will only be called
     * when initial and extend.
     * initial only once and can be add_free in that case
     * extend heap will be used at once, add,then free, it is a waste.
     */
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
    return bp;
}

void add_free(void *bp)
{
    size_t size = GET_SIZE(bp);
    /*      if asize > 128 group into 129~INF       */
    size_t group = size / DSIZE - 2;
    if (group > 127)
    {
        group = 127;
    }
    unsigned int *group_addr = segreg_free + group;

    /* Threre is no block in the group*/
    if (group_addr == (unsigned int *)0)
    {
        SET_PTR(bp, group_addr);
        SET_PTR(bp + WSIZE, (unsigned long *)0);
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
    unsigned long *prev_addr = GET_PTR(bp);
    unsigned long *next_addr = GET_PTR(bp + WSIZE);

    /* The block is the last of the list*/
    if (next_addr == (unsigned long *)0)
    {
        SET_PTR(prev_addr, (unsigned long *)0);
    }
    /* modify the next block*/
    else
    {
        SET_PTR(next_addr, prev_addr);
        SET_PTR(prev_addr, next_addr);
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

    if(heap_listp = 0){
        mem_init();
    }
    /* Ignore supurious requests*/
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs*/
    if (size <= DSIZE)
        asize = DSIZE * 2;
    else
        asize = DSIZE * ((size + (WSIZE) + (DSIZE - 1)) / DSIZE);

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
    size_t group = asize / DSIZE - 2;
    if (group > 127)
        group = 127;

    char found = 0;
    for (int i = group; i < FREE_SIZE && !found; i++)
    {
        unsigned int *group_addr = segreg_free + i;

        if (GET(group_addr) != 0)
        {
            bp = GET_PTR(group_addr);
            while ((unsigned long)bp != addr_head)
            {
                if (GET_SIZE(bp) / DSIZE > asize)
                {
                    found = 1;
                    remove_free(bp);
                    break;
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
    if ((csize - asize) >= (2 * DSIZE))
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
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    void *next_header = HDRP(NEXT_BLKP(bp));
    PUT(next_header, PACK(GET_SIZE(next_header), GET_ALLOC(next_header)));
    bp = coalesce(bp);
    add_free(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
