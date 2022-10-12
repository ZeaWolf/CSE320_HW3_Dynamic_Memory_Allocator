#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include "sfhelper.h"


/* -------------------------------------------------------------------- */
/* Functions to get and set block header and footer. */
sf_header *get_hdrp(sf_block *bp){
	return &(bp->header);
}
sf_footer *get_ftrp(sf_block *bp){
	sf_block *next_bp = get_next_blkp(bp);
	return &(next_bp->prev_footer);
}
sf_header get_header(sf_header *hp){
	return (sf_header)((*hp) ^ MAGIC);
}
void set_header(sf_header *hp, sf_header val){
	sf_header obf_header = (sf_header)(val ^ MAGIC);
	*hp = obf_header;
	return;
}
sf_footer get_footer(sf_footer *fp){
	return (sf_footer)((*fp) ^ MAGIC);
}
void set_footer(sf_footer *fp, sf_footer val){
	sf_footer obf_footer = (sf_footer)(val ^ MAGIC);
	*fp = obf_footer;
	return;
}

/* Functions to parse header information. */
sf_size_t get_payload_size(sf_header *hp){
	sf_header hdr = get_header(hp);
	sf_size_t payload_size = (sf_size_t)((hdr & 0xFFFFFFFF00000000) >> 32);
	return payload_size;
}
sf_size_t get_block_size(sf_header *hp){
	sf_header hdr = get_header(hp);
	sf_size_t block_size = (sf_size_t)(hdr & 0x00000000FFFFFFF0);
	return block_size;
}
unsigned int get_alloc(sf_header *hp){
	sf_header hdr = get_header(hp);
	if(hdr & THIS_BLOCK_ALLOCATED) return 1;
	else return 0;
}
unsigned int get_prev_alloc(sf_header *hp){
	sf_header hdr = get_header(hp);
	if(hdr & PREV_BLOCK_ALLOCATED) return 1;
	else return 0;
}
unsigned int get_in_qklst(sf_header *hp){
	sf_header hdr = get_header(hp);
	if(hdr & IN_QUICK_LIST) return 1;
	else return 0;
}


/* Form a header with given information.*/
sf_header pack_header(sf_size_t payload_size, sf_size_t block_size,
	unsigned int alloc, unsigned int prev_alloc, unsigned int in_qklst){
	sf_header new_header = (sf_header)payload_size;
	new_header = new_header << 32;
	new_header = (new_header|block_size|(alloc*THIS_BLOCK_ALLOCATED)
		|(prev_alloc*PREV_BLOCK_ALLOCATED)|(in_qklst*IN_QUICK_LIST));
	return new_header;
}

/* Get the previous block pointer of current block.*/
sf_block *get_prev_blkp(sf_block *bp){
	sf_footer *prev_ftrp = &(bp->prev_footer);
	sf_size_t prev_bsize = get_block_size((sf_header *)prev_ftrp);
	sf_block *prev_bp = (sf_block *)(((char *)bp) - prev_bsize);
	return prev_bp;
}

/* Get the next block pointer of current block. */
sf_block *get_next_blkp(sf_block *bp){
	sf_header *hdrp = get_hdrp(bp);
	sf_size_t bsize = get_block_size(hdrp);
	sf_block *next_bp = (sf_block *)(((char *)bp) + bsize);
	return next_bp;
}

void set_next_prev_alloc(sf_block *bp, unsigned int prev_alloc){
	sf_block *next_blkp = get_next_blkp(bp);
    sf_header *next_hdrp = get_hdrp(next_blkp);
    sf_size_t ps = get_payload_size(next_hdrp);
    sf_size_t bs = get_block_size(next_hdrp);
    unsigned int ab = get_alloc(next_hdrp);
    unsigned int pab = prev_alloc;
    unsigned int iqb = get_in_qklst(next_hdrp);
    sf_header new_header = pack_header(ps, bs, ab, pab, iqb);
    set_header(next_hdrp, new_header);
    if(ab == 0)
    	set_footer(get_ftrp(next_blkp), (sf_footer)new_header);
    return;
}

/* -------------------------------------------------------------------- */

/* When heap size is 0, initial the heap, quick lists, and free lists. */
int init_heap_and_lists(){

	int i;
	/*Initialize free lists. */
    for(i = 0; i < NUM_FREE_LISTS; i++){
        sf_free_list_heads[i].prev_footer = 0;
        sf_free_list_heads[i].header = 0;
        sf_free_list_heads[i].body.links.next = &(sf_free_list_heads[i]);
        sf_free_list_heads[i].body.links.prev = &(sf_free_list_heads[i]);
    }
    /* Initialize quick lists. */
    for(i = 0; i < NUM_QUICK_LISTS; i++){
        sf_quick_lists[i].length = 0;
        sf_quick_lists[i].first = NULL;
    }

	/* Initialize heap. */
	if(sf_mem_grow() == NULL)
		return -1;

	void *start_ptr = sf_mem_start();
	void *end_ptr = sf_mem_end();

	/* Add Prologue Block. */
	sf_block *prologue_ptr = (sf_block *)start_ptr;
	/* Prologue has no pre footer. (unused)*/
	prologue_ptr->prev_footer = 0;
	/* Prologue Header: 0 payload, 32 block size, 1 alloc bit, 0 pre alloc bit, 0 qklst bit. */
	sf_header prologue_header = pack_header(0, SF_MIN_BLOCK_SIZE, 1, 0, 0);
	set_header(get_hdrp(prologue_ptr), prologue_header);
	/* Prologue has no links and no payload. (unused) */
	prologue_ptr->body.links.prev = NULL;
	prologue_ptr->body.links.next = NULL;

	/* Add Epilogue Header. */
	sf_header *epilogue_ptr = (sf_header *)((char *)end_ptr - sizeof(sf_header));
	/* Epilogue Header: 0 payload, 0 block size, 1 alloca bit, 0 pre alloc bit, 0 qklst bit. */
	sf_header epilogue_header = pack_header(0, 0, 1, 0, 0);
	set_header(epilogue_ptr, epilogue_header);

	/* Add the initial free block between prologue and epilogue to free list */
	sf_block *init_bp = (sf_block *) ((char *)start_ptr + sizeof(sf_block));
	/* Prologue has no footer (unused). */
	init_bp->prev_footer = 0;
	/* Initial Block header: 0 payload,
	   block size = end_ptr - init_bp - size of epilogue header - size of prev footer,
	   0 alloc bit, 1 pre alloc bit, 0 qklst bit */
	sf_size_t init_size = (sf_size_t) ((char *)end_ptr - (char *)init_bp - sizeof(sf_header) - sizeof(sf_footer));
	sf_header init_header = pack_header(0, init_size, 0, 1, 0);

	/* Set header and footer. */
	set_header(get_hdrp(init_bp), init_header);
	set_footer(get_ftrp(init_bp), (sf_footer) init_header);

	/* Insert initial block into free list. */
	if(sf_frlst_insert(init_bp) == -1){
		return -1;
	}

	return 0;
}


/* Try to find a block with given size from quick lists, remove and return it.
   If not found, then return NULL. Do not Update the header and footer yet, just
   return the block pointer.*/
sf_block *sf_qklst_remove(sf_size_t payload_size, sf_size_t block_size){
	/* Block size must be at least 32 and multiple of 16. */
	if(block_size < SF_MIN_BLOCK_SIZE || block_size % SF_ALIGN_SIZE != 0)
		return NULL;

	/* Determine the qindex */
	int qindex = (block_size - SF_MIN_BLOCK_SIZE) / SF_ALIGN_SIZE;

    /* If qindex is too small or too large, return NULL */
    if(qindex < 0 || qindex >= NUM_QUICK_LISTS)
    	return NULL;

    /* If quick list at qindex is empty or too large, return NULL */
    if(sf_quick_lists[qindex].length <= 0
    	|| sf_quick_lists[qindex].length > QUICK_LIST_MAX
    	|| sf_quick_lists[qindex].first == NULL)
    {
    	return NULL;
    }

    /* Remove and return the first block in the quick list at qindex*/
	sf_block *blkp = sf_quick_lists[qindex].first;
	sf_quick_lists[qindex].first = blkp->body.links.next;
	blkp->body.links.next = NULL;
	/* Decrease the length of this quick list by 1. */
	sf_quick_lists[qindex].length--;


	/* Update its header with payload size, block size,
       alloc = 1, keep prev_alloc the same, and in_qklst = 0. */
    /* Ignore footer. */
    sf_header *hdrp = get_hdrp(blkp);
    sf_header header = pack_header(payload_size, block_size, 1, get_prev_alloc(hdrp), 0);
    set_header(hdrp, header);

    /* Set the prev alloc of next block to 1 and keep the rest the same. */
	set_next_prev_alloc(blkp, 1);

	return blkp;
}


/* Try to find a block with given size from free lists, remove and return it.
   If not found, then return NULL. Update the header and pre alloc of next block. */
sf_block *sf_frlst_remove(sf_size_t payload_size, sf_size_t block_size){
	/* Block size must be at least 32 and multiple of 16. */
	if(block_size < SF_MIN_BLOCK_SIZE || block_size % SF_ALIGN_SIZE != 0)
		return NULL;

	/* Determine the findex to start searching.*/
	int findex=-1, i=0, pow_2=1;
	for(i=0; i<NUM_FREE_LISTS-1; i++)
	{
		if(block_size <= pow_2*SF_MIN_BLOCK_SIZE)
		{
			findex = i;
			break;
		}
		pow_2 = pow_2*2;
	}

	/* If not found findex, go search last index */
	if(findex == -1)
		findex = NUM_FREE_LISTS-1;

	/* In case wrong findex, return NULL. */
	if(findex < 0 || findex >= NUM_FREE_LISTS)
		return NULL;

	/* Searching start from findex. */
	sf_block *blkp;
	sf_header *hdrp, header;
	for(i=findex; i<NUM_FREE_LISTS-1; i++)
	{
		/* Iterate each free list.*/
		blkp = &sf_free_list_heads[i];
		while(blkp->body.links.next != &sf_free_list_heads[i])
		{
			blkp = blkp->body.links.next;
			/* If found suitable block, then remove it from list and return */
			if( get_block_size(get_hdrp(blkp)) >= block_size)
			{
				/* Remove and set links.*/
				(blkp->body.links.prev)->body.links.next = blkp->body.links.next;
				(blkp->body.links.next)->body.links.prev = blkp->body.links.prev;
				blkp->body.links.prev = NULL;
				blkp->body.links.next = NULL;

				/* Split block if needed. Function split_block will split the block if possible,
				   return the lower block pointer and insert upper block back to free list. */
				blkp = split_block(blkp, payload_size, block_size);

				/* Update its header with payload size, block size,
			       alloc = 1, keep prev_alloc the same, and in_qklst = 0. */
			    /* Ignore footer. */
			    hdrp = get_hdrp(blkp);
			    header = pack_header(payload_size, block_size, 1, get_prev_alloc(hdrp), 0);
			    set_header(hdrp, header);

			    /* Set the prev alloc of next block to 1 and keep the rest the same. */
    			set_next_prev_alloc(blkp, 1);

				/* Return the block found. */
				return blkp;
			}
		}
	}

	/* If not found any block, return NULL. */
	return NULL;
}


/* Try to insert a block into quick list. If found appropriate index to insert, then
   Update the header to payload size=0, keep block size the same, alloc bit = 1,
   keep pre_alloc bit the same, in_qklst bit = 1. Do not update footer because block
   in quick list has no footer. Also, set the pre_alloc bit of next block to 1.
   If the quick list is full, flush it first. Insert the block at the front of the
   quick list and return 0. If not found appropriate index, return -1. */
int sf_qklst_insert(sf_block *block_ptr){

	/* Get header pointer of this block. */
	sf_header *hdrp = get_hdrp(block_ptr);

	/* Get block size. */
	sf_size_t bsize = get_block_size(hdrp);

	/* Cannot insert if size < 32 or not aligned. */
	if(bsize < SF_MIN_BLOCK_SIZE || bsize % SF_ALIGN_SIZE != 0)
		return -1;

	/* Look for proper qindex to insert */
	int qindex = (bsize - SF_MIN_BLOCK_SIZE) / SF_ALIGN_SIZE;

	/* qindex is too small or too large */
	if(qindex < 0 || qindex >= NUM_QUICK_LISTS)
		return -1;

	/* If quick list at qindex is full, flush it */
    if(sf_quick_lists[qindex].length >= QUICK_LIST_MAX)
    {
    	if(sf_flush_qklst(qindex) == -1)
    	{
    		return -1;
    	}
    }

	/*Update header not footer: */
	sf_header header = pack_header(0, bsize, 1, get_prev_alloc(hdrp), 1);
	set_header(hdrp, header);

	/* Set set the pre_alloc bit of next block to 1. */
	set_next_prev_alloc(block_ptr, 1);

	/* Insert into the front of quick list at qindex. */
	block_ptr->body.links.next = sf_quick_lists[qindex].first;
	sf_quick_lists[qindex].first = block_ptr;
	sf_quick_lists[qindex].length ++;

	return 0;
}


/* Insert a block into free list. Update the header and footer to payload size=0,
   keep block size the same, alloc bit = 0, keep pre_alloc bit the same, in_qklst bit = 0.
   Also, set the pre_alloc bit of next block to 0. Insert the block at the front of the
   free list and return 0. If error occur, return -1. */
int sf_frlst_insert(sf_block *block_ptr){
	/* Get header pointer of this block. */
	sf_header *hdrp = get_hdrp(block_ptr);

	/* Update header and footer: */
	sf_header header = pack_header(0, get_block_size(hdrp), 0, get_prev_alloc(hdrp), 0);
	set_header(hdrp, header);
	set_footer(get_ftrp(block_ptr), (sf_footer)header);

	/* Set pre_alloc of next block to 0. */
	set_next_prev_alloc(block_ptr, 0);

	/* Coalesce previous and next block if possible. */
	sf_block *cblkp = coalesce_block(block_ptr);

	/* Get coalesced header pointer and size. */
	sf_header *chdrp = get_hdrp(cblkp);
	sf_size_t csize = get_block_size(chdrp);

	/* Determine the index of free lists to insert. */
	int findex=-1, i=0, pow_2=1;
	for(i=0; i<NUM_FREE_LISTS-1; i++)
	{
		if(csize <= pow_2*SF_MIN_BLOCK_SIZE)
		{
			findex = i;
			break;
		}
		pow_2 = pow_2*2;
	}
	/* If not found findex, goto last index. */
	if(findex == -1)
		findex = NUM_FREE_LISTS-1;

	/* In case wrong findex, return -1. */
	if(findex < 0 || findex >= NUM_FREE_LISTS)
		return -1;

	/* Insert coalesce block into free list at findex. */
	sf_block *dummy_ptr = &sf_free_list_heads[findex];
	(dummy_ptr->body.links.next)->body.links.prev = cblkp;
	cblkp->body.links.next = dummy_ptr->body.links.next;
	dummy_ptr->body.links.next = cblkp;
	cblkp->body.links.prev = dummy_ptr;

	/* Set the prev alloc bit of next block to 0. */
	set_next_prev_alloc(cblkp, 0);

	return 0;
}

/* Try to split a block. If cannot split, return the original block pointer
   without changing anything. If split made, update the new header and footer
   for both block, and insert the upper block back into free list, and return
   the lower block pointer.*/
sf_block *split_block(sf_block *block_ptr, sf_size_t new_payload_size, sf_size_t new_block_size){
	/* Get header address. */
	sf_header *hdrp = get_hdrp(block_ptr);

	/* Get original size.*/
	sf_size_t original_size = get_block_size(hdrp);

	/* Cannot split smaller block. */
	if(original_size < new_block_size)
		return block_ptr;

	/* Split would cause splinder, so don't split. */
	if((original_size - new_block_size ) < SF_MIN_BLOCK_SIZE)
		return block_ptr;

	/* Split the block into lower block and upper block. */
	sf_block *lower_blkp = block_ptr;

	/* Update only header for lower block. The alloc bit is becomes 1.*/
	set_header(get_hdrp(lower_blkp), pack_header(new_payload_size, new_block_size, 1, get_prev_alloc(get_hdrp(lower_blkp)), 0));
	//set_footer(get_ftrp(lower_blkp), (sf_footer)pack_header(0, size, 0, get_prev_alloc(get_hdrp(lower_blkp)), 0));

	/* Get upper block address. */
	sf_block *upper_blkp = get_next_blkp(lower_blkp);

	/* Update header and footer for upper block. The pre alloc bit is set to 1, because the previous/lower
	   block is soon going to be allocated, so I don't want them to coalesce again.*/
	set_header(get_hdrp(upper_blkp), pack_header(0, (original_size-new_block_size), 0, 1, 0));
	set_footer(get_ftrp(upper_blkp), (sf_footer)pack_header(0, (original_size-new_block_size), 0, 1, 0));

	/* Insert upper block back into free lists.*/
	if(sf_frlst_insert(upper_blkp) == -1)
		return block_ptr;

	/* Return lower block as the block to be allocate. */
	return lower_blkp;
}


/* Coalesce previous and next block if possible.
   If cannot coalesce, then return the original block pointer without any change.
   If coalescing made, update the new header and footer,
   and set pre alloc of next block to 0, then return the coalesced block pointer. */
sf_block *coalesce_block(sf_block *block_ptr){
	/* Get header pointer and block size. */
	sf_block *current_blkp = block_ptr;
	sf_header *current_hdrp = get_hdrp(current_blkp);
	sf_size_t current_size = get_block_size(current_hdrp);

	/* Cannot coalesce allocated block. */
	if(get_alloc(current_hdrp) != 0)
	{
		return current_blkp;
	}

	/* If prev is free too, then coalesce the previous block. */
	if(get_prev_alloc(current_hdrp) == 0)
	{
		/* Get previous block address. */
		sf_block *prev_blkp = get_prev_blkp(current_blkp);

		/* Remove previous block out of free lists. */
		(prev_blkp->body.links.prev)->body.links.next = prev_blkp->body.links.next;
		(prev_blkp->body.links.next)->body.links.prev = prev_blkp->body.links.prev;
		prev_blkp->body.links.prev = NULL;
		prev_blkp->body.links.next = NULL;

		/* Get previous header address. */
		sf_header *prev_hdrp = get_hdrp(prev_blkp);

		/* Get previous block size. */
		sf_size_t prev_size = get_block_size(prev_hdrp);

		/* Update current size. */
		current_size = current_size + prev_size;

		/* Update current block address and header/footer content. */
		current_blkp = prev_blkp;
		set_header(get_hdrp(current_blkp), pack_header(0, current_size, 0, get_prev_alloc(get_hdrp(current_blkp)), 0));
		set_footer(get_ftrp(current_blkp), (sf_footer)pack_header(0, current_size, 0, get_prev_alloc(get_hdrp(current_blkp)), 0));
		/* Set pre alloc bit of next block to 0.*/
		set_next_prev_alloc(current_blkp, 0);
	}

	/* Get next block address and the next header */
	sf_block *next_blkp = get_next_blkp(current_blkp);
	sf_header *next_hdrp = get_hdrp(next_blkp);

	/* If next is free too, then coalesce the next block. */
	if(get_alloc(next_hdrp) == 0)
	{
		/* Remove next block out of free lists. */
		(next_blkp->body.links.prev)->body.links.next = next_blkp->body.links.next;
		(next_blkp->body.links.next)->body.links.prev = next_blkp->body.links.prev;
		next_blkp->body.links.prev = NULL;
		next_blkp->body.links.next = NULL;

		/* Get next block size. */
		sf_size_t next_size = get_block_size(next_hdrp);

		/* Update current size. */
		current_size = current_size + next_size;

		/* Update header/footer content. */
		set_header(get_hdrp(current_blkp), pack_header(0, current_size, 0, get_prev_alloc(get_hdrp(current_blkp)), 0));
		set_footer(get_ftrp(current_blkp), (sf_footer)pack_header(0, current_size, 0, get_prev_alloc(get_hdrp(current_blkp)), 0));
		/* Set pre alloc bit of next block to 0.*/
		set_next_prev_alloc(current_blkp, 0);
	}
	return current_blkp;
}

/* If no appropriate block can be found in free lists, call heap to grow,
   which creates new block and insert it into free list. Also update the new
   epilogue. */
int sf_create_new_page(){

	/* Increase heap size. */
	void *previous_heap_end = sf_mem_grow();
	if(previous_heap_end == NULL)
		return -1;

	/* Old Epilogue and get its prev_alloc bit. */
	sf_header *old_epilogue = (sf_header *) ((char *)previous_heap_end - sizeof(sf_header));
	unsigned int old_pre_alloc = get_prev_alloc(old_epilogue);

	/* Update new Epilogue. */
	void *new_heap_end = sf_mem_end();
	sf_header *new_epilogue = (sf_header *) ((char *)new_heap_end - sizeof(sf_header));

	/* New Epilogue Header: 0 payload, 0 block size, 1 alloca bit, 0 pre alloc bit, 0 qklst bit. */
	sf_header epilogue_header = pack_header(0, 0, 1, 0, 0);
	set_header(new_epilogue, epilogue_header);

	/* Update the new block created. */
	sf_block *new_blkp = (sf_block *)((char *)previous_heap_end - sizeof(sf_header) - sizeof(sf_footer));

	/* New size. */
	sf_size_t new_size = (sf_size_t)((char *)new_heap_end - (char *)new_blkp - sizeof(sf_header) - sizeof(sf_footer));
	/* Set new header and footer. */
	sf_header new_header = pack_header(0, new_size, 0, old_pre_alloc, 0);
	set_header(get_hdrp(new_blkp), new_header);
	set_footer(get_ftrp(new_blkp), (sf_footer)new_header);

	/* Insert new block into free list. */
	if(sf_frlst_insert(new_blkp) == -1){
		return -1;
	}

	return 0;
}

/* When try to insert a block into a quick list, but the quick list is full (reached QUICK_LIST_MAX).
   This function removes all the block in that quick list and insert them into free lists. */
int sf_flush_qklst(int index){

	/* Iterate each block in the quick list at index. */
	sf_block *blkp;
	while(sf_quick_lists[index].first != NULL){
		blkp = sf_quick_lists[index].first;

		/* Remove block from quick list. */
		sf_quick_lists[index].first = blkp->body.links.next;
		sf_quick_lists[index].length--;

		/* Insert the removed block into free list. */
		if(sf_frlst_insert(blkp) == -1)
			return -1;
	}

	return 0;
}