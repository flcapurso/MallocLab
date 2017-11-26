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
    " NoClue Team",
    /* First member's full name */
    " Filippo Capurso ",
    /* First member's email address */
    " flcapurso@kaist.ac.kr",
    /* Second member's full name (leave blank if none) */
    " Henrik Iller Waersted ",
    /* Second member's email address (leave blank if none) */
    " carzita@kaist.ac.kr"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 8

/* ADDITIONAL MACROS */
#define INITIALSIZE 1024 //only data size (no header, footer, padding)
#define ALLOCATED 1
#define FREE 0
#define HEADSIZE 4
#define FOOTSIZE 4
#define INITIALPADDING 4
#define POINTERSIZE 4
#define MINDATASIZE (ALIGN(1))

// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

// Read and write a word at address p
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7) //extracts size byte from 4 byte header or footer
#define GET_ALLOCATED(p) (GET(p) & 0x1) //extracts allocated byte from 4 byte header or footer
#define SET_ALLOC(p,val) (*(unsigned int *)(p) &= ~(val))

#define HEADER(ptr) (ptr - HEADSIZE) //gets header address of ptr
#define FOOTER(ptr) (ptr + GET_SIZE(HEADER(ptr))) //gets footer address of ptr

#define SET_NEXT(ptr, val) (PUT(ptr,val))
#define SET_PREV(ptr, val) (PUT((ptr + POINTERSIZE),val))
#define GET_NEXT(ptr) ((void *) GET(ptr))
#define GET_PREV(ptr) ((void *) GET(ptr + POINTERSIZE))

//#define NEXT(ptr) ((unsigned int *)(ptr) + GET_SIZE(((unsigned int *)(ptr) + FOOTSIZE))) // access next block

#define NEXT(ptr) (ptr + GET_SIZE(HEADER(ptr)) + FOOTSIZE) // access next block
#define PREVIOUS(ptr) (ptr - (HEADSIZE + FOOTSIZE) - GET_SIZE(ptr - (HEADSIZE + FOOTSIZE))) // access previous block footer

void *freeListStart;

static void *coalesce(void *ptr);
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    printf("NEW START\n");
    // Create new heap and initialise free list
    freeListStart = mem_sbrk(ALIGN(INITIALPADDING + HEADSIZE + FOOTSIZE + INITIALSIZE));
    //printf("Start pointer: %p\n", freeListStart);
    // Move free pointer after the header
    freeListStart = (void *) ((char *)freeListStart + (HEADSIZE + FOOTSIZE));
    //printf("Final pointer: %p\n", freeListStart);
    // Set Header
    //*(unsigned int *)(freeListStart - HEADSIZE) = (INITIALSIZE | FREE);
    PUT(HEADER(freeListStart), (INITIALSIZE | FREE));
    //printf("PUT test & GET test -> Size: %d, Allocated?: %d\n", GET_SIZE(HEADER(freeListStart)), GET_ALLOCATED(HEADER(freeListStart)));
    //printf("Compare header value -> %d : %d\n",*(unsigned int *)(freeListStart - HEADSIZE), (INITIALSIZE | FREE));
    // Set Footer
    //*(unsigned int *)(freeListStart + INITIALSIZE) = (INITIALSIZE | FREE);
    PUT(FOOTER(freeListStart), (INITIALSIZE | FREE));
    //printf("Footer -> Size: %d, Allocated?: %d\n", GET_SIZE(FOOTER(freeListStart)), GET_ALLOCATED(FOOTER(freeListStart)));
    //printf("Footer pointer: %p\n", (freeListStart + INITIALSIZE));
    //printf("Compare footer value%d : %d\n",*(unsigned int *)(freeListStart + INITIALSIZE), (INITIALSIZE | FREE));
    // Set 'next' pointer to zero
    //printf("Final pointer: %p\n", freeListStart);
    SET_NEXT(freeListStart, 0);
    SET_PREV(freeListStart, 0);
    //printf("SET pointer test: %p, %p\n", GET_NEXT(freeListStart), GET_PREV(freeListStart));

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    printf("# Malloc -> %zu\n", size);

    //unsigned int requiredTotalSize = ALIGN(size + (HEADSIZE + FOOTSIZE));
    unsigned int requiredDataSize = ALIGN(size);

    // Get First element in free space (if there is any free space)
    if (freeListStart != (void *) -1) {
        void *nextFree = freeListStart; // pointer to first free space, used to itinerate between the next ones
        short matchFound = 0; // has a EXACT match been found
        unsigned int minAvailableSize = UINT_MAX; //save closest size
        void *bestFitPointer; // save pointer to the saved space
        unsigned int currentSize = 0; // holds next free space size to compare with saved one
        int numberFree = 0; // just tprint how many free spaces have been found
        // Loop to check all free spaces (stops if pointer has not been assigned(which means end of list) or exact match found)
        while ( ((int)nextFree != 0) && (matchFound == 0) ) {
            numberFree++;
            printf("\t%d\n", numberFree);
            // get size of the next free space
            currentSize = GET_SIZE(HEADER(nextFree));
            // see if it matches exactly
            if (minAvailableSize == requiredDataSize) {
                matchFound = 1;
                minAvailableSize = currentSize;
                bestFitPointer = nextFree;
            }
            // or if is the closest found till now
            else if ((minAvailableSize > currentSize) && (currentSize > requiredDataSize)) {
                minAvailableSize = currentSize;
                bestFitPointer = nextFree;
            }
            // move on to the next pointer for next itineration (if exact match has not yet been found)
            nextFree = GET_NEXT(nextFree);
        }

        // If no match was found, expand heap
        if (minAvailableSize == UINT_MAX) {
            printf("No match found\n");
            void *endHeap = mem_heap_hi();
            // +1 since mem_heap_hi() returns LAST byte, not end of heap
            endHeap = (void *) ((char*)endHeap + 1);
            unsigned int prevSize = 0;
            short prevAlloc = GET_ALLOCATED(endHeap - (HEADSIZE + FOOTSIZE));

            // if last block is free, expand only by required
            if (!prevAlloc){
                printf("\tExpand only by required\n");
                prevSize = GET_SIZE(endHeap - (HEADSIZE + FOOTSIZE));
                printf("\t\t prevSize: %d; required: %d. %p\n", prevSize, requiredDataSize, endHeap);
                void *addedHeap = mem_sbrk(requiredDataSize - prevSize);
                if (addedHeap == (void *)-1)
                    return NULL;
                // continue only if there is still memory (no errors)
                else {
                    void *newAllocated = (void *) ((char *)addedHeap - prevSize - (HEADSIZE + FOOTSIZE));
                    void *prevNXTpointer = GET_NEXT(newAllocated);
                    void *prevPRVpointer = GET_PREV(newAllocated);

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
                        SET_PREV(prevPRVpointer, 0);
                    // if in middle of list
                    else {
                        SET_NEXT(prevPRVpointer, ((unsigned int) prevNXTpointer));
                        SET_PREV(prevNXTpointer, ((unsigned int) prevPRVpointer));
                    }

                    // update header and footer
                    PUT(HEADER(newAllocated),((requiredDataSize - prevSize) | ALLOCATED));
                    PUT((newAllocated + requiredDataSize + prevSize),((requiredDataSize - prevSize) | ALLOCATED));
                    printf("%zu : %d ; %p\n",size, (requiredDataSize + (HEADSIZE + FOOTSIZE)), newAllocated);
                    return newAllocated;
                }
            }
            // if last block is allocated, simply expand
            else {
                printf("\tSimply expand\n");
                void *addedHeap = mem_sbrk(requiredDataSize + (HEADSIZE + FOOTSIZE));
                if (addedHeap == (void *)-1)
                    return NULL;
                // continue only if there is still memory (no errors)
                else {
                    PUT(HEADER(addedHeap),(requiredDataSize | ALLOCATED));
                    PUT((addedHeap + requiredDataSize), (requiredDataSize | ALLOCATED));
                    printf("%zu : %d ; %p\n",size, (requiredDataSize+(HEADSIZE + FOOTSIZE)), addedHeap);
                    return addedHeap;
                }
            }
        }

        // If free space was found
        else {
            printf("Free space found \n");
            void *oldNXTpointer = GET_NEXT(bestFitPointer);
            void *oldPRVpointer = GET_PREV(bestFitPointer);
            // if exact match or negligible additional free space, simply assign it
            if ( (matchFound == 1) || ((minAvailableSize - requiredDataSize) < ((HEADSIZE + FOOTSIZE) + MINDATASIZE)) ) {
                printf("\tExact or almost \n");
                // if current block is first in the free list
                if ((int)oldPRVpointer == 0) {
                    // and if also last, set 'freeListStart' to -1
                    if ((int)oldNXTpointer == 0)
                        freeListStart = (void *) -1;
                    else
                        freeListStart = oldNXTpointer;
                }
                // if current block is last in the free list
                else if ((int)oldNXTpointer == 0)
                    SET_PREV((oldPRVpointer), 0);
                // if in middle of list
                else {
                    SET_NEXT(oldPRVpointer, (unsigned int)oldNXTpointer);
                    SET_PREV(oldNXTpointer, (unsigned int)oldPRVpointer);
                }
                //allocate memory
                SET_ALLOC(HEADER(bestFitPointer), ALLOCATED);
                SET_ALLOC((bestFitPointer + requiredDataSize), ALLOCATED);
                printf("%zu : %d ; %p\n",size, (requiredDataSize+(HEADSIZE + FOOTSIZE)), bestFitPointer);
            }
            // if additional space remains, store it as free space
            else {
             printf("\tSplit and free remaining space \n");
                void *newFree = (void *) ((char *)bestFitPointer + requiredDataSize + (HEADSIZE + FOOTSIZE));
                PUT(HEADER(newFree), ((minAvailableSize - requiredDataSize - (HEADSIZE + FOOTSIZE)) | FREE));
                PUT((newFree + requiredDataSize), ((minAvailableSize - requiredDataSize - (HEADSIZE + FOOTSIZE)) | FREE));
                SET_NEXT(newFree, (unsigned int)oldNXTpointer);
                SET_PREV(newFree, (unsigned int)oldPRVpointer);
                //allocate memory
                PUT(HEADER(bestFitPointer), (requiredDataSize | ALLOCATED));
                PUT((bestFitPointer + requiredDataSize), (requiredDataSize | ALLOCATED));
                printf("%zu : %d ; %p\n",size, (requiredDataSize+(HEADSIZE + FOOTSIZE)), bestFitPointer);
            }
            return bestFitPointer;
        }
    }
    // if there is no free space, simply expand heap
    else {
        printf("No free space -> Expand heap \n");
        void *addedHeap = mem_sbrk(requiredDataSize + (HEADSIZE + FOOTSIZE));
        if (addedHeap == (void *)-1)
            return NULL;
        else {
            PUT(HEADER(addedHeap), (requiredDataSize | ALLOCATED));
            PUT((addedHeap + requiredDataSize), (requiredDataSize | ALLOCATED));
            printf("%zu : %d ; %p\n",size, (requiredDataSize+(HEADSIZE + FOOTSIZE)), addedHeap);
            return addedHeap;
        }
    }

    /*
    void *p = mem_sbrk(requiredSize);
    printf("%zu : %d ; %p\n",size, requiredSize, p);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + (HEADSIZE + FOOTSIZE));
    }
    */
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    printf("# Free -> %p\n", ptr);
    size_t size = GET_SIZE(HEADER(ptr));
    PUT(HEADER(ptr), PACK(size, FREE));
    PUT(FOOTER(ptr), PACK(size, FREE));
    coalesce(ptr);
}

static void *coalesce (void *ptr) {
    printf("# coalesce -> %p\n", ptr);
    size_t prevBlockAllocated = GET_ALLOCATED(FOOTER(PREVIOUS(ptr)));
    size_t nextBlockAllocated = GET_ALLOCATED(HEADER(NEXT(ptr)));

    size_t size = GET_SIZE(HEADER(ptr));

    if(prevBlockAllocated && nextBlockAllocated) {
        // nothing happens because they are both allocated
        return ptr;
    }
    else if(prevBlockAllocated && !nextBlockAllocated) {
        // prevBlock is allocated and nextBlock is free
        size += GET_SIZE(HEADER(NEXT(ptr)));
        PUT(HEADER(ptr), PACK(size, FREE));
        PUT(FOOTER(ptr), PACK(size, FREE));
        return ptr;
    }
    else if (!prevBlockAllocated && nextBlockAllocated) {
        // prevBlock is free and nextBlock is allocated
        size += GET_SIZE(HEADER(PREVIOUS(ptr)));
        PUT(HEADER(PREVIOUS(ptr)), PACK(size, FREE));
        PUT(FOOTER(ptr), PACK(size, FREE));
        return (PREVIOUS(ptr));
    }
    else {
        // both are free
        size += GET_SIZE(HEADER(PREVIOUS(ptr))) + GET_SIZE(HEADER(NEXT(ptr)));
        PUT(HEADER(PREVIOUS(ptr)), PACK(size, FREE));
        PUT(FOOTER(NEXT(ptr)), PACK(size, FREE));
        return (PREVIOUS(ptr));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    printf("# Realloc -> %p\n", ptr);
    void *oldptr = ptr;
    void *newptr = mm_malloc(size);
    size_t copySize;

    // if PTR is NULL the call is equivalent to mm_malloc(size)
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // if size is 0 the call is equivalent to mm_free(ptr)
    if(size == 0) {
        mm_free(ptr);
        newptr = 0;
        return NULL;
    }

    copySize = GET_SIZE(HEADSIZE(ptr));
    if (size < copySize) {
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(ptr);
    return newptr;
}