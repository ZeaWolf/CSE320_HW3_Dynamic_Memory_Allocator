#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == ((bp->header ^ MAGIC) & 0xfffffff0))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

/*
 * Assert the total number of quick list blocks of a specified size.
 * If size == 0, then assert the total number of all quick list blocks.
 */
void assert_quick_list_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_QUICK_LISTS; i++) {
	sf_block *bp = sf_quick_lists[i].first;
	while(bp != NULL) {
	    if(size == 0 || size == ((bp->header ^ MAGIC) & 0xfffffff0)) {
		cnt++;
		if(size != 0) {
		    // Check that the block is in the correct list for its size.
		    int index = (size - 32) >> 4;
		    cr_assert_eq(index, i, "Block %p (size %ld) is in wrong quick list for its size "
				 "(expected %d, was %d)",
				 &bp->header, (bp->header ^ MAGIC) & 0xfffffff0, index, i);
		}
	    }
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of quick list blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of quick list blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz = sizeof(int);
	int *x = sf_malloc(sz);

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");
	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((bp->header >> 32) & 0xffffffff,
		  "Malloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (bp->header >> 32) & 0xffffffff, sz);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(944, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;

	void *x = sf_malloc(4032);
	cr_assert_not_null(x, "x is NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(98304);

	cr_assert_null(x, "x is not NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(24528, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_quick, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 32, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(48, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(864, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 200, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(208, 1);
	assert_free_block_count(704, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
	/* void *w = */ sf_malloc(sz_w);
	void *x = sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	sf_free(x);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(384, 1);
	assert_free_block_count(528, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT) {
        size_t sz_u = 200, sz_v = 150, sz_w = 50, sz_x = 150, sz_y = 200, sz_z = 250;
	void *u = sf_malloc(sz_u);
	/* void *v = */ sf_malloc(sz_v);
	void *w = sf_malloc(sz_w);
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_quick_list_block_count(0, 1);
	assert_free_block_count(0, 3);
	assert_free_block_count(208, 2);
	assert_free_block_count(928, 1);

	// First block in list should be the most recently freed block not in quick list.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(&bp->header, (char *)y - 8,
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, &bp->header, (char *)y - 8);
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
	void *x = sf_malloc(sz_x);
	/* void *y = */ sf_malloc(sz_y);
	x = sf_realloc(x, sz_x1);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((bp->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((bp->header ^ MAGIC) & 0xfffffff0) == 96,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  (bp->header ^ MAGIC) & 0xfffffff0, 96);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == sz_x1,
		  "Realloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), sz_x1);

	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(32, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(816, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int) * 20, sz_y = sizeof(int) * 16;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((bp->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((bp->header ^ MAGIC) & 0xfffffff0) == 96,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  (bp->header ^ MAGIC) & 0xfffffff0, 96);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == sz_y,
		  "Realloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), sz_y);

	// There should be only one free block.
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(880, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(double) * 8, sz_y = sizeof(int);
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((bp->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((bp->header ^ MAGIC) & 0xfffffff0) == 32,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  (bp->header ^ MAGIC) & 0xfffffff0, 32);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == sz_y,
		  "Realloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), sz_y);

	// After realloc'ing x, we can return a block of size 48
	// to the freelist.  This block will go into the main freelist and be coalesced.
	// Note that we don't put split blocks into the quick lists because their sizes are not sizes
	// that were requested by the client, so they are not very likely to satisfy a new request.
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(944, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

void assert_block_header(void *pp, sf_size_t payload_size, sf_size_t block_size,
	unsigned int alloc, unsigned int prev_alloc, unsigned int in_qklst){
	sf_block *blkp =(sf_block *)((char *)pp - sizeof(sf_header) - sizeof(sf_footer));
	sf_header current_header = blkp->header ^ MAGIC;
	cr_assert( ((current_header>>32) & 0xffffffff ) == payload_size,
		"Payload size (%ld) not what was expected (%ld)", (current_header>>32) & 0xffffffff, payload_size);
	cr_assert( (current_header & 0xfffffff0) == block_size,
		"Block size (%ld) not what was expected (%ld)", current_header & 0xfffffff0, block_size);
	cr_assert( ((current_header>>2) & 0x1) == alloc,
		"Allocated bit (%ld) not what was expected (%ld)", (current_header>>2) & 0x1, alloc);
	cr_assert( ((current_header>>1) & 0x1) == prev_alloc,
		"Previous allocated bit (%ld) not what was expected (%ld)", (current_header>>1) & 0x1, prev_alloc);
	cr_assert( (current_header & 0x1) == in_qklst,
		"In quick list bit (%ld) not what was expected (%ld)", current_header & 0x1, in_qklst);
}

void assert_sf_statistics(double exp_inter_frag, double exp_peak_util){
	double itfg = sf_internal_fragmentation();
	double pkuz = sf_peak_utilization();
	cr_assert_eq(itfg, exp_inter_frag, "Wrong number of internal fragmentation (exp=%f, found=%f)",
		     exp_inter_frag, itfg);
	cr_assert_eq(pkuz, exp_peak_util, "Wrong number of peak utilization (exp=%f, found=%f)",
		     exp_peak_util, pkuz);
}

Test(sfmm_student_suite, student_test_1, .timeout = TEST_TIMEOUT) {
	void **temp = (void **)sf_malloc(23*sizeof(void *));
	cr_assert_not_null(temp, "temp is NULL!");
	assert_block_header(temp, 184, 192, 1, 1, 0);
	/* Test flush qklst. sf_malloc and free small block 23 times*/
	int i;
	for(i=0; i<23; i++){
		/* payload size: 30, 32, 34, 36, 38, 40, ..., 66, 68, 70, 72, 74 */
		/* block size: 6x48, 8x64, 8x80, 1x96*/
		temp[i] = sf_malloc(30+(2*i));
		cr_assert_not_null(temp[i], "temp[%d] is NULL!", i);
		if(i<6)
			assert_block_header(temp[i], 30+(2*i), 48, 1, 1, 0);
		else if(i<14)
			assert_block_header(temp[i], 30+(2*i), 64, 1, 1, 0);
		else if(i<22)
			assert_block_header(temp[i], 30+(2*i), 80, 1, 1, 0);
		else
			assert_block_header(temp[i], 30+(2*i), 96, 1, 1, 0);

	}

	for(i=0; i<23; i++){
		sf_free(temp[i]);
		if(i<6)
		{
			if(i == 5)
				assert_block_header(temp[i], 0, 48, 1, 0, 1);
			else
				assert_block_header(temp[i], 0, 48, 1, 1, 1);
		}
		else if(i<14)
		{
			if(i == 11)
				assert_block_header(temp[i], 0, 64, 1, 0, 1);
			else
				assert_block_header(temp[i], 0, 64, 1, 1, 1);
		}
		else if(i<22)
		{
			if(i == 19)
				assert_block_header(temp[i], 0, 80, 1, 0, 1);
			else
				assert_block_header(temp[i], 0, 80, 1, 1, 1);
		}
		else
			assert_block_header(temp[i], 0, 96, 1, 1, 1);
	}

	sf_free(temp);
	/* temp should be free to free list and coalesce with next block of size 5x48=240*/
	assert_block_header(temp, 0, 432, 0, 1, 0);

	/* At the end, there should be 8 blocks in qklst. */
	assert_quick_list_block_count(0, 8);
	/* 1 blocks with size 48.*/
	assert_quick_list_block_count(48, 1);
	/* 3 blocks with size 64.*/
	assert_quick_list_block_count(64, 3);
	/* 3 blocks with size 80.*/
	assert_quick_list_block_count(80, 3);
	/* 1 blocks with size 96.*/
	assert_quick_list_block_count(96, 1);

	/* 4 blocks in free lists. */
	assert_free_block_count(0, 4);
	assert_free_block_count(432, 1); // 192 + 5*48 = 432
	assert_free_block_count(320, 1); // 5*64
	assert_free_block_count(400, 1); // 5*80
	assert_free_block_count(272, 1); // 2000 - 192 - (6*48 + 8*64 + 8*80 + 96) = 272

	assert_sf_statistics(0.0, (double)1380/2048);
}

Test(sfmm_student_suite, student_test_2, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	assert_sf_statistics(0.0, 0.0);
	size_t sz_x = 100*sizeof(int), sz_y = 15*sizeof(int), sz_z = 80*sizeof(int);
	void *x = sf_malloc(sz_x);

	/* Only one allocated block with payload size = 400 and block size = 416. */
	/* Only one free block of block size 560 in free list. */
	/* Current Statistics: */
	/* Internal fragmentation = 400/416 */
	/* Peak utilization = 400/1024 */
	cr_assert_not_null(x, "x is NULL!");
	assert_block_header(x, 400, 416, 1, 1, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(560, 1);
	assert_sf_statistics((double)400 / (double)416, (double)400/1024);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	/* Reallocate x to y (reallocate to a smaller) */
	/* Only one allocated block with payload size = 60 and block size = 80. */
	/* y should the same pointer as x */
	/* Only one free block of block size 896 in free list. */
	/* Current Statistics: */
	/* Internal fragmentation = 60/80 */
	/* Peak utilization = 400/1024 */
	void *y = sf_realloc(x, sz_y);
	cr_assert_not_null(y, "y is NULL!");
	assert_block_header(x, 60, 80, 1, 1, 0);
	assert_block_header(y, 60, 80, 1, 1, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(896, 1);
	assert_sf_statistics((double)60 / (double)80, (double)400/1024);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	/* Reallocate y to z (reallocate to a larger). */
	/* Only one allocated block with payload size = 320 and block size = 336. */
	/* y should the same pointer as x and they should be in quick list with size 80. */
	/* Only one free block of block size 560 in free list. */
	/* Current Statistics: */
	/* Internal fragmentation = 320/336 */
	/* Peak utilization = 400/1024 */
	void *z = sf_realloc(y, sz_z);
	cr_assert_not_null(z, "z is NULL!");
	assert_block_header(x, 0, 80, 1, 1, 1);
	assert_block_header(y, 0, 80, 1, 1, 1);
	assert_block_header(z, 320, 336, 1, 1, 0);
	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(80, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(560, 1);
	assert_sf_statistics((double)320 / (double)336, (double)400/1024);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_student_suite, student_test_3, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *p0 = sf_realloc(NULL, 120);
	cr_assert_null(p0, "p0 is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");

	sf_errno = 0;
	void *p1 = sf_malloc(1999);
	cr_assert_not_null(p1, "p1 is NULL!");
	assert_block_header(p1, 1999, 2016, 1, 1, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(1008, 1);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	sf_errno = 0;
	void *p2 = sf_realloc((void *)((char *)p1 - 5), 245);
	cr_assert_null(p2, "p2 is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");

	sf_errno = 0;
	void *p3 = sf_realloc(sf_mem_start(), 90);
	cr_assert_null(p3, "p3 is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");

	sf_errno = 0;
	void *p4 = sf_realloc(sf_mem_end(), 110);
	cr_assert_null(p4, "p4 is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");

	sf_errno = 0;
	void *p5 = sf_malloc(110);
	cr_assert_not_null(p5, "p5 is NULL!");
	assert_block_header(p5, 110, 128, 1, 1, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(880, 1);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	sf_errno = 0;
	void *pn = sf_realloc(p1, 0);
	cr_assert_null(pn, "pn is not NULL!");
	assert_block_header(p1, 0, 2016, 0, 1, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(2016, 1);
	assert_free_block_count(880, 1);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	sf_errno = 0;
	sf_free(p5);
	assert_block_header(p5, 0, 128, 1, 0, 1);
	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(128, 1);
	assert_free_block_count(0, 2);
	assert_free_block_count(2016, 1);
	assert_free_block_count(880, 1);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	sf_errno = 0;
	void *p6 = sf_realloc(p1, 98);
	cr_assert_null(p6, "p6 is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");

	sf_errno = 0;
	void *p7 = sf_realloc(p5, 69);
	cr_assert_null(p7, "p7 is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}

Test(sfmm_student_suite, student_test_4, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(1990);
	cr_assert_not_null(x, "x is NULL!");
	assert_block_header(x, 1990, 2000, 1, 1, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	assert_sf_statistics((double)1990/(double)2000, (double)1990/2048);

	void *y = sf_realloc(x, 2000);
	cr_assert_neq(x, y, "x and y are the same pointer!");
	cr_assert_not_null(y, "y is NULL!");
	assert_block_header(x, 0, 2000, 0, 1, 0);
	assert_block_header(y, 2000, 2016, 1, 0, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(2000, 1);
	assert_free_block_count(32, 1);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	sf_free(y);
	assert_block_header(x, 0, 4048, 0, 1, 0);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(4048, 1);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_student_suite, student_test_5, .timeout = TEST_TIMEOUT, .signal = SIGABRT) {
	sf_errno = 0;
	assert_sf_statistics(0.0, 0.0);
	/* sf_malloc(0) should return NULL without setting sf_errno. */
	void *p0 = sf_malloc(0);
	cr_assert_null(p0, "p0 is not NULL!");
	cr_assert(sf_errno == 0, "sf_errno is not 0!");

	/* sf_free should abort, test recieve signal = SIGABRT */
	sf_free(p0);
}
