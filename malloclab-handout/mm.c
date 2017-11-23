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
#include <limits.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "NoClue Team",
    /* First member's full name */
    "Filippo Capurso",
    /* First member's email address */
    "flcapurso@kaist.ac.kr",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* ADDITIONAL MACROS */
#define INITIALSIZE 1024 //only data size (no header, footer, padding)
#define ALLOCATED 1
#define FREE 0
#define HEADSIZE 4
#define FOOTSIZE 4
#define INITIALPADDING 4
#define POINTERSIZE 4

void *freeListStart;
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    printf("NEW START\n");
    // Create new heap and initialise free list
    freeListStart = mem_sbrk(INITIALSIZE);
    printf("Start pointer: %p\n", freeListStart);
    // Move free pointer after the header
    freeListStart = (void *) ((char *)freeListStart + SIZE_T_SIZE);
    printf("Final pointer: %p\n", freeListStart);
    // Set Header
    *(unsigned int *)(freeListStart - HEADSIZE) = (INITIALSIZE | FREE);
    printf("Compare header value -> %d : %d\n",*(unsigned int *)(freeListStart), (INITIALSIZE | FREE));
    // Set Footer
    *(unsigned int *)(freeListStart + INITIALSIZE) = (INITIALSIZE | FREE);
    printf("Footer pointer: %p\n", (freeListStart + INITIALSIZE));
    printf("Compare footer value%d : %d\n",*(unsigned int *)(freeListStart + INITIALSIZE), (INITIALSIZE | FREE));
    // Set 'next' pointer to zero
    *(unsigned int *)(freeListStart) = 0;
    *(unsigned int *)(freeListStart + POINTERSIZE) = 0;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //unsigned int requiredTotalSize = ALIGN(size + SIZE_T_SIZE);
    unsigned int requiredDataSize = ALIGN(size);

    // Get 'next' pointer stored in free space
    void *nextFree = freeListStart;
    short matchFound = 0;
    unsigned int minAvailableSize = UINT_MAX;
    void *bestFitPointer;
    unsigned int currentSize = 0;
    int numberFree = 0;
    while ( ((int)nextFree != 0) && (matchFound == 0) ) {
        numberFree++;
        printf("\t%d\n", numberFree);
        currentSize = (*(unsigned int *)nextFree) & ~0x7;
        if (minAvailableSize == requiredDataSize) {
            matchFound = 1;
            minAvailableSize = currentSize;
            bestFitPointer = nextFree;
        }
        else if ((minAvailableSize > currentSize) && (currentSize > requiredDataSize)) {
            minAvailableSize = currentSize;
            bestFitPointer = nextFree;
        }
        nextFree = (void *) *(unsigned int *)(nextFree);
    }

    // If no match was found, expand heap
    if (minAvailableSize == UINT_MAX) {
        void *endHeap = mem_heap_hi();
        unsigned int prevSize = 0;
        short prevAlloc = (*(unsigned int *)(endHeap - SIZE_T_SIZE)) & 0x1;

        // if last block is free, expand only by required
        if (!prevAlloc){
            prevSize = (*(unsigned int *)(endHeap - SIZE_T_SIZE)) & ~0x7;
            void *addedHeap = mem_sbrk(requiredDataSize - prevSize);
            if (addedHeap == (void *)-1)
                return NULL;
            // continue only if there is still memory (no errors)
            else {
                void *newAllocated = (void *) ((char *)addedHeap - prevSize);
                void *prevPRVpointer = (void *) *(unsigned int *)(newAllocated);
                void *prevNXTpointer = (void *) *(unsigned int *)(newAllocated + POINTERSIZE);

                // if last block is first in the free list
                if ((int)prevPRVpointer == 0) {
                    // and if also last, set 'freeListStart' to -1
                    if ((int)prevNXTpointer == 0)
                        freeListStart = (void *) -1;
                    else
                        freeListStart = prevNXTpointer;
                }
                // if last block is last in the free list
                else if ((int)prevNXTpointer == 0)
                    *(unsigned int *)(prevPRVpointer) = 0;
                // if in middle of list
                else {
                    *(unsigned int *)(prevPRVpointer) = *(unsigned int *)prevNXTpointer;
                    *(unsigned int *)(prevNXTpointer) = *(unsigned int *)prevPRVpointer;
                }

                // update header and footer
                *(unsigned int *)(newAllocated - HEADSIZE) = ((requiredDataSize + prevSize) | ALLOCATED);
                *(unsigned int *)(newAllocated + requiredDataSize + prevSize) = ((requiredDataSize + prevSize) | ALLOCATED);
                printf("%d : %d ; %p\n",size, (requiredDataSize + SIZE_T_SIZE), newAllocated);
                return newAllocated;
            }
        }
        // if last block is allocated, simply expand
        else {
            void *addedHeap = mem_sbrk(requiredDataSize + SIZE_T_SIZE);
            if (addedHeap == (void *)-1)
                return NULL;
            // continue only if there is still memory (no errors)
            else {
                *(unsigned int *)(addedHeap - HEADSIZE) = (requiredDataSize | ALLOCATED);
                *(unsigned int *)(addedHeap + requiredDataSize) = (requiredDataSize | ALLOCATED);
                printf("%d : %d ; %p\n",size, (requiredDataSize+SIZE_T_SIZE), addedHeap);
                return addedHeap;
            }
        }
    }
    // If match was found, simply assign it
    else {
        //allocate memory
        *(unsigned int *)(bestFitPointer - HEADSIZE) = (requiredDataSize | ALLOCATED);
        *(unsigned int *)(bestFitPointer + requiredDataSize) = (requiredDataSize | ALLOCATED);
        printf("%d : %d ; %p - 4\n",size, (requiredDataSize+SIZE_T_SIZE), bestFitPointer);

        // NEED TO DO: Re-organise free list
        return bestFitPointer;
    }
    /*
    void *p = mem_sbrk(requiredSize);
    printf("%d : %d ; %p\n",size, requiredSize, p);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
    */
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t prevBlock = GET_ALLOC(FOOTER(PREVIOUS(ptr)));
    size_t nextBlock = GET_ALLOC(HEADER(NEXT(ptr)));
    size_t size = GET_SIZE(HEADER(ptr));

    PUT(HEADER(ptr), PACK(size, 0));
    PUT(FOOTER(ptr), PACK(size, 0));

    if(prevBlock && nextBlock) {
        // nothing happens because they are both allocated
    }
    else if(!prevBlock && nextBlock) {
        // prevBlock is free and nextBlock is allocated
        size += GET_SIZE(HEADER(NEXT))
    }
    else if (prevBlock && !nextBlock) {
        // prevBlock is allocated and nextBlock is free
    }
    else {
        // both are free

    }
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
