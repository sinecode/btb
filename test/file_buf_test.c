#include "file_buf_test.h"

#include <stdio.h>
#include <string.h>

#include "../src/defer.h"
#include "../src/file_buf.h"

#pragma GCC diagnostic ignored "-Wformat"

void test_read_file()
{
  file_buf *f = read_file("test/data/test.txt");
  DEFER({ free_file_buf(f); });
  if (f == NULL) {
    fprintf(stderr, "error in read_file\n");
  }
  char wantText[] = "This is a test file\n";
  if (f->size != strlen(wantText)) {
    fprintf(stderr, "wrong file_buf->size: got %d, want %d\n", f->size, strlen(wantText));
  }
  if (memcmp(f->data, wantText, strlen(wantText)) != 0) {
    fprintf(stderr, "wrong file content: got %.*s, want %s\n", f->size, f->data, wantText);
  }
}
