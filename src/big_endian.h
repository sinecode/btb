#pragma once

#include <stdbool.h>
#include <stddef.h>

// Convert the input unsigned long integer n to a big endian representation
// of buf_size bytes in the buffer buf.
// It returns true if it succeeds.
bool lu_to_big_endian(unsigned long n, unsigned char* buf, size_t buf_size);

// Convert the input big endian representation of buf_size bytes
// in a long unsigned integer.
unsigned long big_endian_to_lu(unsigned char* buf, size_t buf_size);
