#pragma once

#include <pthread.h>
#include <stdio.h>

#include "bitfield.h"

// A thread-safe data structure to extract piece indexes.
typedef struct pieces_pool
{
  bitfield* current_bitfield;
  size_t num_pieces;
  pthread_mutex_t _mutex;
} pieces_pool;

// Init a pieces pool with n piece indexes from 0 to n-1.
pieces_pool* init_pieces_pool(size_t n);

void free_pieces_pool(pieces_pool*);

// Get and mark as "done" the first "undone" piece index.
// It returns x > num_pieces if all pieces are done.
size_t get_piece_index(pieces_pool*);

// Set the input index as "undone",
// So it can be extracted using get_piece_index.
void mark_as_undone(pieces_pool*, size_t piece_index);

bool is_done(pieces_pool*);

// Return the number of "undone" pieces left.
size_t get_num_undone_pieces(pieces_pool*);
