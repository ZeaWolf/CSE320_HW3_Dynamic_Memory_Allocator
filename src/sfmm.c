/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "sfhelper.h"


void *sf_malloc(sf_size_t size) {
    sf_block *target_block_ptr = NULL;

    /* If the request size is 0, then return NULL without setting sf_errno. */
    if(size == 0)
    {
        return NULL;
    }

    /* If the heap size is 0, then initialize heap, quick lists, and free lists. */
    void *heap_start = sf_mem_start();
    void *heap_end = sf_mem_end();
    if(heap_start == heap_end)
    {
        /* Set global variables to 0. */
        total_payload_size = 0;
        total_allocated_block_size = 0;
        max_aggregate_payload = 0;
        if(init_heap_and_lists() == -1)
        {
            sf_errno = ENOMEM;
            return NULL;
        }
    }

    /* First, determine the required block size (header+payload+padding). */
    sf_size_t bsize = size + sizeof(sf_header);                         // add header size 8 bytes.
    if(bsize < SF_MIN_BLOCK_SIZE)
    {
        bsize = SF_MIN_BLOCK_SIZE;                                      // minimum block size is 32.
    }
    else if((bsize % SF_ALIGN_SIZE) != 0)
    {
        bsize = bsize + (SF_ALIGN_SIZE - (bsize % SF_ALIGN_SIZE));      // add padding.
    }

    /* Run while loop until a target block is found. */
    while(target_block_ptr == NULL)
    {
        /* Check the quick lists. */
        target_block_ptr = sf_qklst_remove(size, bsize);

        /* If target block not found in quick list, then go check free lists. */
        if(target_block_ptr == NULL)
        {
            target_block_ptr = sf_frlst_remove(size, bsize);
        }

        /* If target block not found in free list, then call sf_mem_grow(). */
        if(target_block_ptr == NULL)
        {
            if(sf_create_new_page() == -1)
            {
                sf_errno = ENOMEM;
                return NULL;
            }
        }

    }

    /* Update global variable. */
    total_payload_size = total_payload_size + size;
    total_allocated_block_size = total_allocated_block_size + bsize;
    if(total_payload_size  > max_aggregate_payload)
        max_aggregate_payload = total_payload_size;

    // /* Once got the target block point to be allocate.*/
    // /* Update its header with payload size, block size,
    //    alloc = 1, keep prev_alloc the same, and in_qklst = 0. */
    // /* Remove the footer / set footer to 0. */
    // sf_header *target_hdrp = get_hdrp(target_block_ptr);
    // sf_header target_header = pack_header(size, bsize, 1, get_prev_alloc(target_hdrp), 0);
    // set_header(target_hdrp, target_header);
    // sf_footer *target_ftrp = get_ftrp(target_block_ptr);
    // set_footer(target_ftrp, (sf_footer)0);

    // /* Set the prev alloc of next block to 1 and keep the rest the same. */
    // set_next_prev_alloc(target_block_ptr, 1);

    /* Get payload pointer to be returned. */
    void *payload_ptr = (void *)(&(target_block_ptr->body.payload));

    /* Return the payload starting pointer to the application. */
    return payload_ptr;
}

void sf_free(void *pp) {

    /* Verify that the pointer being passed to your function belongs to an allocated block. */

    /* The pointer is NULL. */
    if(pp == NULL)
    {
        abort();
    }
    /* The pointer is not 16-byte aligned. */
    if( ((unsigned long)pp & 0xF) != 0)
    {
        abort();
    }
    /* After XOR'ing the stored header with MAGIC: */

    /* Get block pointer, header pointer, and block size. */
    sf_block *pp_blkp = (sf_block *) ( (char *)pp - sizeof(sf_header) - sizeof(sf_footer) );
    sf_header *pp_hdrp = get_hdrp(pp_blkp);

    /* Get block and payload size. */
    sf_size_t pp_block_size = get_block_size(pp_hdrp);
    sf_size_t pp_payload_size = get_payload_size(pp_hdrp);

    /* The block size is less than the minimum block size of 32. */
    if(pp_block_size < SF_MIN_BLOCK_SIZE)
    {
        abort();
    }

    /* The block size is not a multiple of 16 */
    if((pp_block_size % SF_ALIGN_SIZE) != 0)
    {
        abort();
    }

    /* The payload size is 0 or is larger than block size. */
    if(pp_payload_size <= 0 || pp_payload_size >= pp_block_size)
    {
        abort();
    }

    /* The header of the block is before the start of the first block of the heap,
       or the footer of the block is after the end of the last block in the heap. */
    if( ((void *)get_hdrp(pp_blkp) <= sf_mem_start()) || ((void *)get_ftrp(pp_blkp) >= sf_mem_end()))
    {
        abort();
    }

    /* The allocated bit in the header is 0. */
    if(get_alloc(pp_hdrp) == 0)
    {
        abort();
    }

    /* The qklst bit in the header is not 0. */
    if(get_in_qklst(pp_hdrp) != 0)
    {
        abort();
    }

    /* The prev_alloc field in the header is 0, indicating that the previous block is free,
       but the alloc field of the previous block header is not 0. */
    if(get_prev_alloc(pp_hdrp) == 0)
    {
        if(get_alloc(get_hdrp(get_prev_blkp(pp_blkp))) != 0)
        {
            abort();
        }
    }

    if(sf_qklst_insert(pp_blkp) == -1)
    {
        if(sf_frlst_insert(pp_blkp) == -1)
        {
            abort();
        }
    }

    /* Update global variable. */
    total_payload_size = total_payload_size - pp_payload_size;
    total_allocated_block_size = total_allocated_block_size - pp_block_size;
    if(total_payload_size  >  max_aggregate_payload)
        max_aggregate_payload = total_payload_size;

    return;
}

void *sf_realloc(void *pp, sf_size_t rsize) {
    /* Verify that the pointer being passed to your function belongs to an allocated block. */

    /* The pointer is NULL. */
    if(pp == NULL)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    /* The pointer is not 16-byte aligned. */
    if( ((unsigned long)pp & 0xF) != 0)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    /* After XOR'ing the stored header with MAGIC: */

    /* Get block pointer, header pointer. */
    sf_block *pp_blkp = (sf_block *) ( (char *)pp - sizeof(sf_header) - sizeof(sf_footer) );
    sf_header *pp_hdrp = get_hdrp(pp_blkp);

    /* Get block and payload size. */
    sf_size_t pp_block_size = get_block_size(pp_hdrp);
    sf_size_t pp_payload_size = get_payload_size(pp_hdrp);

    /* The block size is less than the minimum block size of 32. */
    if(pp_block_size < SF_MIN_BLOCK_SIZE)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    /* The block size is not a multiple of 16 */
    if((pp_block_size % SF_ALIGN_SIZE) != 0)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    /* The payload size is 0 or is larger than block size. */
    if(pp_payload_size <= 0 || pp_payload_size >= pp_block_size)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    /* The header of the block is before the start of the first block of the heap,
       or the footer of the block is after the end of the last block in the heap. */
    if(((void *)get_hdrp(pp_blkp) <= sf_mem_start()) || ((void *)get_ftrp(pp_blkp) >= sf_mem_end()))
    {
        sf_errno = EINVAL;
        return NULL;
    }

    /* The allocated bit in the header is 0. */
    if(get_alloc(pp_hdrp) == 0)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    /* The qklst bit in the header is not 0. */
    if(get_in_qklst(pp_hdrp) != 0)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    /* If sf_realloc is called with a valid pointer and a size of 0 it should free
       the allocated block and return NULL without setting sf_errno. */
    if(rsize == 0)
    {
        sf_free(pp);
        return NULL;
    }

    /* Reallocating to a Same Size. */
    if(pp_payload_size == rsize)
    {
        return pp;
    }

    /* Determine the new block size needed. */
    sf_size_t new_bsize = rsize + sizeof(sf_header);                    // add header size 8 bytes.
    if(new_bsize < SF_MIN_BLOCK_SIZE)
    {
        new_bsize = SF_MIN_BLOCK_SIZE;                                  // minimum block size is 32.
    }
    else if((new_bsize % SF_ALIGN_SIZE) != 0)
    {
        new_bsize = new_bsize + (SF_ALIGN_SIZE - (new_bsize % SF_ALIGN_SIZE));      // add padding.
    }

    /* Reallocating to a Larger Size. */
    if(pp_payload_size < rsize)
    {
        /* new block required. */
        if(pp_block_size < new_bsize)
        {
            /* 1. Call sf_malloc to obtain a larger block. */
            void *new_ptr = sf_malloc(rsize);

            /* If sf_malloc returns NULL, sf_realloc must also return NULL. */
            if(new_ptr == NULL)
            {
                return NULL;
            }

            /* 2. Call memcpy to copy the data in the block given by the client
               to the block returned by sf_malloc.  Be sure to copy the entire
               payload area, but no more. */
            memcpy(new_ptr, pp, (size_t)pp_payload_size);

            /* 3. Call sf_free on the block given by the client (inserting into a quick list
               or main freelist and coalescing if required). */
            sf_free(pp);

            /* 4. Return the block given to you by sf_malloc to the client. */
            return new_ptr;
        }

        /* If that block fits the new payload size */
        else
        {
            /* Set the pp header to new payload size */
            set_header(pp_hdrp, pack_header(rsize,
                get_block_size(pp_hdrp), get_alloc(pp_hdrp), get_prev_alloc(pp_hdrp), get_in_qklst(pp_hdrp)));

            /* Update global variable. */
            total_payload_size = total_payload_size - pp_payload_size + rsize;
            total_allocated_block_size = total_allocated_block_size - pp_block_size + get_block_size(pp_hdrp);
            if(total_payload_size  >  max_aggregate_payload)
                max_aggregate_payload = total_payload_size;

            /* Return original pp */
            return pp;
        }
    }

    /* Reallocating to a Smaller Size. */
    if(pp_payload_size > rsize)
    {
        /* Try to split block. */
        sf_block *sblkp = split_block(pp_blkp, rsize, new_bsize);

        /* In case did not split, set the header to new payload size */
        sf_header *shdrp = get_hdrp(sblkp);
        set_header(shdrp, pack_header(rsize,
            get_block_size(shdrp), get_alloc(shdrp), get_prev_alloc(shdrp), get_in_qklst(shdrp)));

        /* Update global variable. */
        total_payload_size = total_payload_size - pp_payload_size + rsize;
        total_allocated_block_size = total_allocated_block_size - pp_block_size + get_block_size(shdrp);
        if(total_payload_size  >  max_aggregate_payload)
            max_aggregate_payload = total_payload_size;

        void *sptr = (void *)(&(sblkp->body.payload));
        return sptr;
    }

    return NULL;
}

double sf_internal_fragmentation() {
    double inter_frag;
    if(total_allocated_block_size <= 0){
        inter_frag = 0;
    }
    else{
        inter_frag = total_payload_size/total_allocated_block_size;
    }

    return inter_frag;
}

double sf_peak_utilization() {
    double peak_util;
    unsigned long heap_size = (unsigned long)sf_mem_end() - (unsigned long)sf_mem_start();
    if(heap_size <= 0){
        peak_util = 0;
    }
    else{
        peak_util = max_aggregate_payload / heap_size;
    }
    return peak_util;
}
