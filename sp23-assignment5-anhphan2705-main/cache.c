#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int num_queries = 0;
static int num_hits = 0;
static int clock = 0;

// Create a cache with the specified number of entries
int cache_create(int num_entries) {
	// Check if the number of entries is valid and if the cache is already enabled
	if (num_entries < 2 || num_entries > 4096 || cache_enabled()) {
		return -1;
	}

	// Allocate memory for the cache and set the cache size
	cache = calloc(num_entries, sizeof(cache_entry_t));
	cache_size = num_entries;

	// Return success
    return 1;
}

// Destroy the cache
int cache_destroy(void) {
	// Check if the cache is enabled
	if (!cache_enabled()) {
		return -1;
	}

	// Free the memory used by the cache and reset cache-related variables
	free(cache);
	cache = NULL;
	cache_size = 0;
	num_queries = 0;
	num_hits = 0;

	// Return success
    return 1;
}


// Look up a block in the cache
int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
	// Increment the number of cache queries
	num_queries++;

	// Check if the cache is enabled, if the buffer is valid, and if the disk and block numbers are within a valid range
	if (!cache_enabled() || buf == NULL || disk_num < 0 || block_num < 0 || disk_num > 15 || block_num > 255) {
		return -1;
	}

	// Loop through the cache and check if the block is in the cache
	for (int i = 0; i < cache_size; i++) {
		if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
			// If the block is in the cache, copy the block data to the buffer, update the access time, and return success
			num_hits++;
			memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
			cache[i].access_time = ++clock;
			return 1;
		}
	}
	
	// If the block is not in the cache, return failure
  	return -1;
}


void cache_update(int disk_num, int block_num, const uint8_t *buf) {
	// Look for the cache entry corresponding to the given disk block
	for (int i = 0; i < cache_size; i++) {
		if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
			// Update the cache entry with the new block contents and access time
			memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
			cache[i].access_time = ++clock;
			return;
		}
	}
	// If the cache entry is not found, do nothing
}


int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
    // Check if the cache is enabled and the input parameters are valid
    if (!cache_enabled() || buf == NULL || disk_num < 0 || block_num < 0 || disk_num > 15 || block_num > 255) {
        return -1;
    }

    int least_used = 0;
    // Look for an existing cache entry for the given disk and block number
    for(int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            return -1; // Entry already exists, return an error
        }
        // Find the least recently used entry
        if (cache[i].access_time < cache[least_used].access_time) {
            least_used = i;
        }
    }
    // Insert the new cache entry in the least recently used slot
    cache[least_used].valid = true;
    cache[least_used].disk_num = disk_num;
    cache[least_used].block_num = block_num;
    memcpy(cache[least_used].block, buf, JBOD_BLOCK_SIZE);
    cache[least_used].access_time = 1;
    return 1;
}


bool cache_enabled(void) {
	return cache != NULL && cache_size > 0;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}