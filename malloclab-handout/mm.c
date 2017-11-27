/*
 * mm.c - An improved implementation of explicit lists for malloc, free and realloc
 * 
 * This system simulates malloc(), free() and realloc() using explicit lists, connecting free block
 * according to the most efficient and least expensive operations based on utilasation and performance
 * instead of FIFO or other theories. It also implements a variable called "maxAvailableSize" to keep track
 * of the maximum available free block size, in order to skip going through the free list if the required
 * malloc size is larger. Finally the realloc function was only completed partially and still has a lot of
 * room for improvement.
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
#define INITIALSIZE 512 //only data size (no header, footer, padding)
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
#define SET_ALLOC(p) (*(unsigned int *)(p) |= 0x1) // set block as allocated
#define SET_FREE(p) (*(unsigned int *)(p) &= ~0x1) // set block as free

#define HEADER(ptr) (ptr - HEADSIZE) //gets header address of ptr
#define FOOTER(ptr) (ptr + GET_SIZE(HEADER(ptr))) //gets footer address of ptr

#define SET_NEXT(ptr, val) (PUT(ptr,val)) //sets next pointer
#define SET_PREV(ptr, val) (PUT((ptr + POINTERSIZE),val)) // sets prev pointer
#define GET_NEXT(ptr) ((void *) GET(ptr)) // gets next pointer (as a pointer)
#define GET_PREV(ptr) ((void *) GET(ptr + POINTERSIZE))// gets previous pointer (as a pointer)

#define NEXT(ptr) (ptr + GET_SIZE(HEADER(ptr)) + (HEADSIZE + FOOTSIZE)) // access next block
#define PREVIOUS(ptr) (ptr - (HEADSIZE + FOOTSIZE) - GET_SIZE(ptr - (HEADSIZE + FOOTSIZE))) // access previous block

void *freeListStart; // pointer to start of free list
unsigned int maxAvailableSize; // used to keep track of maximum available free block size

static void *coalesce(void *ptr);
static void *reserveAllocSpace(void *ptr, void *prevBlock, void *nextBlock, short prevBlockAllocated, short nextBlockAllocate);
static void connectFreeList(void *NXTpointer, void *PRVpointer);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    maxAvailableSize = UINT_MAX;
    // Create new heap and initialise free list
    freeListStart = mem_sbrk(ALIGN(INITIALPADDING + HEADSIZE + FOOTSIZE + INITIALSIZE));
    // Move free pointer after the header
    freeListStart = (void *) ((char *)freeListStart + (HEADSIZE + FOOTSIZE));
    // Set Header
    PUT(HEADER(freeListStart), (INITIALSIZE | FREE));
    // Set Footer
    PUT(FOOTER(freeListStart), (INITIALSIZE | FREE));
    // Set 'next' and 'prev' pointers to zero
    SET_NEXT(freeListStart, 0);
    SET_PREV(freeListStart, 0);

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    unsigned int requiredDataSize = ALIGN(size);
    // Get First element in free space (if there is any free space)
    if (freeListStart != (void *) -1) {
        void *nextFree = freeListStart; // pointer to first free space, used to itinerate between the next ones
        short matchFound = 0; // has match been found
        short exactMatch = 0; // has a EXACT match been found
        unsigned int closestSize = UINT_MAX;//save closest size
        void *bestFitPointer; // save pointer to the saved space
        unsigned int currentSize = 0; // holds next free space size to compare with saved one
        int numberFree = 0; // just tprint how many free spaces have been found

        // Loop to check all free spaces (stops if pointer has not been assigned(which means end of list) or exact match found)
        if (maxAvailableSize >= requiredDataSize) {
            while ( ((int)nextFree != 0) && (exactMatch == 0)) {
                numberFree++;
                // get size of the next free space
                currentSize = GET_SIZE(HEADER(nextFree));
                
                if (maxAvailableSize < currentSize) {
                    maxAvailableSize = currentSize;
                }
                // see if it matches exactly
                if (currentSize == requiredDataSize) {
                    exactMatch = 1;
                    matchFound = 1;
                    closestSize = currentSize;
                    bestFitPointer = nextFree;
                }
                // or if is the closest found till now
                else if ((closestSize > currentSize) && (currentSize > requiredDataSize)) {
                    matchFound = 1;
                    closestSize = currentSize;
                    bestFitPointer = nextFree;
                }
                // move on to the next pointer for next itineration (if exact match has not yet been found)
                nextFree = GET_NEXT(nextFree);
            }
        }
        // If no match was found, expand heap
        if (matchFound == 0) {
            void *endHeap = mem_heap_hi();

            // +1 since mem_heap_hi() returns LAST byte, not end of heap
            endHeap = (void *) ((char*)endHeap + 1);
            unsigned int prevSize = 0;
            short prevAlloc = GET_ALLOCATED(endHeap - (HEADSIZE + FOOTSIZE));

            // if last block is free, expand only by required
            if (!prevAlloc){
                prevSize = GET_SIZE(endHeap - (HEADSIZE + FOOTSIZE));
                void *addedHeap = mem_sbrk(requiredDataSize - prevSize);
                if (addedHeap == (void *)-1) {
                    return NULL;
                }
                // continue only if there is still memory (no errors)
                else {
                    void *newAllocated = (void *) ((char *)addedHeap - prevSize - (HEADSIZE + FOOTSIZE));
                    void *prevNXTpointer = GET_NEXT(newAllocated);
                    void *prevPRVpointer = GET_PREV(newAllocated);

                    // if last block is first in the free list
                    if ((int)prevPRVpointer == 0) {
                        // and if also last, set 'freeListStart' to -1
                        if ((int)prevNXTpointer == 0) {
                            freeListStart = (void *)(-1);
                        }
                        else{
                            freeListStart = prevNXTpointer;
                            SET_PREV(prevNXTpointer, 0);
                        }
                    }
                    // if last block is last in the free list
                    else if ((int)prevNXTpointer == 0){
                        SET_NEXT(prevPRVpointer, 0);
                    }
                    // if in middle of list
                    else {
                        SET_NEXT(prevPRVpointer, ((unsigned int) prevNXTpointer));
                        SET_PREV(prevNXTpointer, ((unsigned int) prevPRVpointer));
                    }
                    // update header and footer
                    PUT(HEADER(newAllocated),((requiredDataSize) | ALLOCATED));
                    PUT((newAllocated + requiredDataSize),((requiredDataSize) | ALLOCATED));

                    // Reset maxAvailable size if it was allocated
                    if (prevSize == maxAvailableSize) {
                        maxAvailableSize = UINT_MAX;
                    }
                    return newAllocated;
                }
            }
            // if last block is allocated, simply expand
            else {

                void *addedHeap = mem_sbrk(requiredDataSize + (HEADSIZE + FOOTSIZE));
                if (addedHeap == (void *)-1) {
                    return NULL;
                }
                // continue only if there is still memory (no errors)
                else {
                    PUT(HEADER(addedHeap),(requiredDataSize | ALLOCATED));
                    PUT((addedHeap + requiredDataSize), (requiredDataSize | ALLOCATED));

                    return addedHeap;
                }
            }
        }
        // If free space was found
        else {
            void *oldNXTpointer = GET_NEXT(bestFitPointer);
            void *oldPRVpointer = GET_PREV(bestFitPointer);

            // if exact match or negligible additional free space, simply assign it
            if ( (exactMatch == 1) || ((closestSize - requiredDataSize) < ((HEADSIZE + FOOTSIZE) + MINDATASIZE)) ) {
                // if current block is first in the free list
                if ((int)oldPRVpointer == 0) {
                    // and if also last, set 'freeListStart' to -1
                    if ((int)oldNXTpointer == 0){
                        freeListStart = (void *) -1;
                    }
                    else {
                        freeListStart = oldNXTpointer;
                        SET_PREV(oldNXTpointer,0);
                    }
                }
                // if current block is last in the free list
                else if ((int)oldNXTpointer == 0){
                    SET_NEXT((oldPRVpointer), 0);
                }
                // if in middle of list
                else {
                    SET_NEXT(oldPRVpointer, (unsigned int)oldNXTpointer);
                    SET_PREV(oldNXTpointer, (unsigned int)oldPRVpointer);
                }
                //allocate memory
                SET_ALLOC(HEADER(bestFitPointer));
                SET_ALLOC(bestFitPointer + requiredDataSize);
            }
            // if additional space remains, store it as free space
            else {
                void *newFree = (void *) ((char *)bestFitPointer + requiredDataSize + (HEADSIZE + FOOTSIZE));
                unsigned int freeSize = (closestSize - requiredDataSize - (HEADSIZE + FOOTSIZE));
                PUT(HEADER(newFree), (freeSize | FREE));
                PUT(FOOTER(newFree), (freeSize | FREE));
                SET_NEXT(newFree, (unsigned int)oldNXTpointer);
                SET_PREV(newFree, (unsigned int)oldPRVpointer);

                // if at start of list
                if ((int)oldPRVpointer == 0){
                    freeListStart = newFree;
                }
                else{
                    SET_NEXT(oldPRVpointer, (unsigned int)newFree);
                }
                // if at end of list
                if ((int)oldNXTpointer != 0) {
                    SET_PREV(oldNXTpointer, (unsigned int)newFree);
                }

                //allocate memory
                PUT(HEADER(bestFitPointer), (requiredDataSize | ALLOCATED));
                PUT((bestFitPointer + requiredDataSize), (requiredDataSize | ALLOCATED));
            }
            // Reset maxAvailable size if it was allocated
            if (closestSize == maxAvailableSize) {
                maxAvailableSize = UINT_MAX;
            }
            return bestFitPointer;
        }
    }
    // if there is no free space, simply expand heap
    else {
        void *addedHeap = mem_sbrk(requiredDataSize + (HEADSIZE + FOOTSIZE));

        if (addedHeap == (void *)-1) {
            return NULL;
        }
        else {
            PUT(HEADER(addedHeap), (requiredDataSize | ALLOCATED));
            PUT((addedHeap + requiredDataSize), (requiredDataSize | ALLOCATED));

            return addedHeap;
        }
    }
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    coalesce(ptr);
}

static void *coalesce (void *ptr) {

    unsigned int size = GET_SIZE(HEADER(ptr));

    void *prevBlock = PREVIOUS(ptr);
    void *nextBlock = NEXT(ptr);
    short prevBlockAllocated = GET_ALLOCATED(HEADER(prevBlock));
    short nextBlockAllocated = GET_ALLOCATED(HEADER(nextBlock));

    // if at start
    if ( ((int)mem_heap_lo() + INITIALPADDING + HEADSIZE) == ((int)ptr) ){
        prevBlockAllocated = 1;
    }
    // if at end. (did not use "else if" since heap can be composed of a single free block, ie. is at beginning and end)
    if ( ((int)mem_heap_hi() + 1 - (HEADSIZE + FOOTSIZE)) == ((int)ptr + size) ){
        nextBlockAllocated = 1;
    }

    // nothing happens because they are both allocated
    if(prevBlockAllocated && nextBlockAllocated) {
        // Set header and footer
        PUT(HEADER(ptr), PACK(size, FREE));
        PUT(FOOTER(ptr), PACK(size, FREE));

        //Set pointers
        if (freeListStart != (void *) -1) {
            void* oldFirstFree = freeListStart;
            SET_PREV(oldFirstFree, (unsigned int)ptr);
            SET_NEXT(ptr, (unsigned int)oldFirstFree);
        }
        else {
            SET_NEXT(ptr, 0);
        }
        freeListStart = ptr;
        SET_PREV(ptr, 0);

        // update maxAvailableSize if size is new max
        if (size > maxAvailableSize) {
            maxAvailableSize = size;
        }
        return ptr;
    }

    // prevBlock is allocated and nextBlock is free
    else if(prevBlockAllocated && !nextBlockAllocated) {
        //Set header and footer
        size += ( (HEADSIZE + FOOTSIZE) + GET_SIZE(HEADER(nextBlock)) );
        PUT(HEADER(ptr), PACK(size, FREE));
        PUT(FOOTER(nextBlock), PACK(size, FREE));

        // Set pointers
        void *NXTpointer = GET_NEXT(nextBlock);
        void *PRVpointer = GET_PREV(nextBlock);
        SET_NEXT(ptr, (int)NXTpointer);
        SET_PREV(ptr, (int)PRVpointer);
        if ((int)PRVpointer == 0){
            freeListStart = ptr;
        }
        else {
            SET_NEXT(PRVpointer, (int)ptr);
        }
        if ((int)NXTpointer != 0) {
            SET_PREV(NXTpointer, (int)ptr);
        }

        // update maxAvailableSize if size is new max
        if (size > maxAvailableSize) {
            maxAvailableSize = size;
        }
        return ptr;
    }

    // prevBlock is free and nextBlock is allocated
    else if (!prevBlockAllocated && nextBlockAllocated) {
        // Set Header and footer
        size += ( (HEADSIZE + FOOTSIZE) + GET_SIZE(HEADER(prevBlock)) );
        PUT(HEADER(prevBlock), PACK(size, FREE));
        PUT(FOOTER(ptr), PACK(size, FREE));
        //No need to change pointers

        // update maxAvailableSize if size is new max
        if (size > maxAvailableSize) {
            maxAvailableSize = size;
        }
        return (prevBlock);
    }

    // both are free
    else {
        // Set header and footer
        size += ( 2*(HEADSIZE + FOOTSIZE) + GET_SIZE(HEADER(prevBlock)) + GET_SIZE(HEADER(nextBlock)) );
        PUT(HEADER(prevBlock), PACK(size, FREE));
        PUT(FOOTER(nextBlock), PACK(size, FREE));

        // Set Pointers
        void *nextNXTpointer = GET_NEXT(nextBlock);
        void *nextPRVpointer = GET_PREV(nextBlock);
        if ((int)nextPRVpointer == 0){
            freeListStart = nextNXTpointer;
            SET_PREV(nextNXTpointer, 0);
        }
        else if ((int)nextNXTpointer == 0) {
            SET_NEXT(nextPRVpointer, 0);
        }
        else{
            SET_NEXT(nextPRVpointer, (int)nextNXTpointer);
            SET_PREV(nextNXTpointer, (int)nextPRVpointer);
        }

        // update maxAvailableSize if size is new max
        if (size > maxAvailableSize) {
            maxAvailableSize = size;
        }
        return (prevBlock);
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t requiredSize)
{
    requiredSize = ALIGN(requiredSize);
    // if PTR is NULL the call is equivalent to mm_malloc(requiredSize)
    if (ptr == NULL) {
        return mm_malloc(requiredSize);
    }

    // if requiredSize is 0 the call is equivalent to mm_free(ptr)
    if(requiredSize == 0) {
        mm_free(ptr);
        return NULL;
    }

    unsigned int currentSize = GET_SIZE(HEADER(ptr));
    unsigned int availableSize; // used to store the total available size around block (including block)

    void *prevBlock = PREVIOUS(ptr);
    void *nextBlock = NEXT(ptr);
    short prevBlockAllocated = GET_ALLOCATED(HEADER(prevBlock));
    short nextBlockAllocated = GET_ALLOCATED(HEADER(nextBlock));

    // if at start of heap
    if ( ((int)mem_heap_lo() + INITIALPADDING + HEADSIZE) == ((int)ptr) ){
        prevBlockAllocated = 1;
    }
    // if at end of heap
    if ( ((int)mem_heap_hi() + 1 - (HEADSIZE + FOOTSIZE)) == ((int)ptr + currentSize) ){
        nextBlockAllocated = 1;
    }

    //calculate availableSize including adiecent free blocks
    if (prevBlockAllocated && nextBlockAllocated) {
        availableSize = currentSize;
    }
    else if (prevBlockAllocated && !nextBlockAllocated) {
        availableSize = currentSize + GET_SIZE(HEADER(nextBlock)) + (HEADSIZE + FOOTSIZE);
    }
    else if (!prevBlockAllocated && nextBlockAllocated) {
        availableSize = currentSize + GET_SIZE(HEADER(prevBlock)) + (HEADSIZE + FOOTSIZE);
    }
    else {
        availableSize = currentSize + GET_SIZE(HEADER(nextBlock)) + GET_SIZE(HEADER(prevBlock)) + (HEADSIZE + FOOTSIZE);
    }

    // If availableSize is enough, no heap extension is required
    if (availableSize >= requiredSize) {
        // Coalesce blocks and return pointer to start
        void *newptr = reserveAllocSpace(ptr, prevBlock, nextBlock, prevBlockAllocated, nextBlockAllocated);

        // Only move data if required (when prev block is free)
        if (prevBlockAllocated) {
            return newptr;
        }
        else {
            if (requiredSize > currentSize){
                memcpy(newptr, ptr, currentSize);
            }
            else {
                memcpy(newptr, ptr, requiredSize);
            }
        }

        // if additional space remains, split and free
        if ( availableSize > (requiredSize + MINDATASIZE)) {
            // Calculate size and pointer of free space
            void *newFree = (void *) ((char *)newptr + requiredSize + (HEADSIZE + FOOTSIZE));
            unsigned int freeSize = (availableSize - requiredSize - (HEADSIZE + FOOTSIZE));
            // Set header and footer of new free space
            PUT(HEADER(newFree), (freeSize | FREE));
            PUT(FOOTER(newFree), (freeSize | FREE));
            // Redefine header and footer of reallocated space
            PUT(HEADER(newptr), (requiredSize | ALLOCATED));
            PUT(FOOTER(newptr), (requiredSize | ALLOCATED));

            // Set pointers
            if (freeListStart == (void *)-1) {
                freeListStart = newFree;
                SET_NEXT(newFree, 0);
            }
            else {
                SET_NEXT(newFree, (unsigned int)freeListStart);
                SET_PREV(freeListStart, (unsigned int)newFree);
                freeListStart = newFree;
            }

            SET_PREV(newFree, 0);

            // Reassign maxAvailableSize if new free space is new max
            if (freeSize >= maxAvailableSize) {
                maxAvailableSize = freeSize;
            }
        }
        return newptr;
    }
    // If available size is not enough, call malloc and then free
    else {
        void *newptr;
        newptr = mm_malloc(requiredSize);

        // The original block is left untouched if realloc fails
        if(!newptr) {
            return 0;
        }

        memcpy(newptr, ptr, requiredSize);
        mm_free(ptr);
        return newptr;
    }
}
/*
 * Function to coalesce one given block with the free adiecent ones without modifying the data between the resulting block header and footer
 */
static void *reserveAllocSpace (void *ptr, void *prevBlock, void *nextBlock, short prevBlockAllocated, short nextBlockAllocated) {


    unsigned int size = GET_SIZE(HEADER(ptr));
    unsigned int extraSize;

    // prevBlock is allocated and nextBlock is free
    if(prevBlockAllocated && !nextBlockAllocated) {
        extraSize = GET_SIZE(HEADER(nextBlock));
        if (extraSize == maxAvailableSize) {
            maxAvailableSize = UINT_MAX;
        }
        size += ( (HEADSIZE + FOOTSIZE) + extraSize );
        PUT(HEADER(ptr), PACK(size, ALLOCATED));
        PUT(FOOTER(nextBlock), PACK(size, ALLOCATED));
        void *NXTpointer = GET_NEXT(nextBlock);
        void *PRVpointer = GET_PREV(nextBlock);
        connectFreeList(NXTpointer, PRVpointer);
        return ptr;
    }
    // prevBlock is free and nextBlock is allocated
    else if (!prevBlockAllocated && nextBlockAllocated) {
        extraSize = GET_SIZE(HEADER(prevBlock));
        if (extraSize == maxAvailableSize) {
            maxAvailableSize = UINT_MAX;
        }
        size += ( (HEADSIZE + FOOTSIZE) + extraSize );
        PUT(HEADER(prevBlock), PACK(size, ALLOCATED));
        PUT(FOOTER(ptr), PACK(size, ALLOCATED));
        void *NXTpointer = GET_NEXT(prevBlock);
        void *PRVpointer = GET_PREV(prevBlock);
        connectFreeList(NXTpointer, PRVpointer);
        return (prevBlock);
    }
    // both are free
    else {
        extraSize = GET_SIZE(HEADER(prevBlock));
        if (extraSize == maxAvailableSize) {
            maxAvailableSize = UINT_MAX;
        }
        size += 2*(HEADSIZE + FOOTSIZE) + extraSize;
        extraSize = GET_SIZE(HEADER(nextBlock));
        if (extraSize == maxAvailableSize) {
            maxAvailableSize = UINT_MAX;
        }
        size += extraSize;
        PUT(HEADER(prevBlock), PACK(size, ALLOCATED));
        PUT(FOOTER(nextBlock), PACK(size, ALLOCATED));
        void *nextNXTpointer = GET_NEXT(nextBlock);
        void *nextPRVpointer = GET_PREV(nextBlock);
        connectFreeList(nextNXTpointer, nextPRVpointer);
        void *prevNXTpointer = GET_NEXT(prevBlock);
        void *prevPRVpointer = GET_PREV(prevBlock);
        connectFreeList(prevNXTpointer, prevPRVpointer);

        return (prevBlock);
    }
}
 /*
  * Function to connect two pieces of the free list that where connected by a now allocated block.
  */
static void connectFreeList(void *NXTpointer, void *PRVpointer) {
        if ((int)PRVpointer == 0){
            if ((int)NXTpointer == 0) {
                freeListStart = (void *) -1;
            }
            else {
                freeListStart = NXTpointer;
                SET_PREV(NXTpointer, 0);
            }
        }
        else if ((int)NXTpointer == 0) {
            SET_NEXT(PRVpointer, 0);
        }
        else {
            SET_NEXT(PRVpointer, (int)NXTpointer);
            SET_PREV(NXTpointer, (int)PRVpointer);
        }
}