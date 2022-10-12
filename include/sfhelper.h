#ifndef SFHELPER_H
#define SFHELPER_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SF_MIN_BLOCK_SIZE	32
#define SF_ALIGN_SIZE		16

double total_payload_size;
double total_allocated_block_size;
double max_aggregate_payload;

sf_header *get_hdrp(sf_block *bp);
sf_footer *get_ftrp(sf_block *bp);
sf_header get_header(sf_header *hp);
void set_header(sf_header *hp, sf_header val);
sf_footer get_footer(sf_footer *fp);
void set_footer(sf_footer *fp, sf_footer val);

/* Parse header information. */
sf_size_t get_payload_size(sf_header *hp);
sf_size_t get_block_size(sf_header *hp);
unsigned int get_alloc(sf_header *hp);
unsigned int get_prev_alloc(sf_header *hp);
unsigned int get_in_qklst(sf_header *hp);

sf_header pack_header(sf_size_t payload_size, sf_size_t block_size,
	unsigned int alloc, unsigned int prev_alloc, unsigned int in_qklst);

sf_block *get_prev_blkp(sf_block *bp);
sf_block *get_next_blkp(sf_block *bp);

void set_next_prev_alloc(sf_block *bp, unsigned int prev_alloc);


int init_heap_and_lists();

sf_block *sf_qklst_remove(sf_size_t payload_size, sf_size_t block_size);

sf_block *sf_frlst_remove(sf_size_t payload_size, sf_size_t block_size);

int sf_qklst_insert(sf_block *block_ptr);

int sf_frlst_insert(sf_block *block_ptr);

sf_block *split_block(sf_block *block_ptr, sf_size_t new_payload_size, sf_size_t new_block_size);

sf_block *coalesce_block(sf_block *block_ptr);

int sf_create_new_page();

int sf_flush_qklst(int index);

#endif