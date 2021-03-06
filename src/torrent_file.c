#include "torrent_file.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_buf.h"
#include "hash.h"
#include "tokenizer.h"

#pragma GCC diagnostic ignored "-Wpointer-sign"

torrent_file *init_torrent_file()
{
  torrent_file *t = malloc(sizeof(torrent_file));
  t->announce = NULL;
  t->comment = NULL;
  t->info_hash = NULL;
  t->num_pieces = 0;
  t->piece_hashes = NULL;
  t->piece_length = 0;
  t->length = 0;
  t->name = NULL;
  return t;
}

bool parse_torrent_file(torrent_file *tr, const char *fname)
{
  fprintf(stdout, "parsing %s\n", fname);
  file_buf *buf = read_file(fname);
  tokenizer *tk = init_tokenizer(buf->data, buf->size);
  free_file_buf(buf);

  bool ok = true;
  // Pointer to the location where the info section starts,
  // which is the 'd' character after the 'info' key.
  char *info_start = NULL;
  // Value of the list_dict_stack when the key info is encountered.
  // This value should be checked to identify the end of the info dictionary.
  long info_start_stack_value = 0;
  // Pointer to the location where the info section ends,
  // which is the 'e' character.
  char *info_end = NULL;
  while (true) {
    int err = next(tk);
    if (err == TOKENIZER_END) {
      break;
    }
    if (err == TOKENIZER_MALFORMED_STRING) {
      fprintf(stderr, "tokenizer failed with code %d\n", err);
      ok = false;
      break;
    }
    if (tk->token_size == strlen("announce") &&
        memcmp(tk->token, "announce", strlen("announce")) == 0) {
      /* --- parse announce --- */
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      if (memcmp(tk->token, "s", tk->token_size) != 0) {
        fprintf(stderr, "expected s token, got %.*s\n", (int)tk->token_size, tk->token);
        ok = false;
        break;
      }
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      tr->announce = realloc(tr->announce, tk->token_size + 1);
      memcpy(tr->announce, tk->token, tk->token_size);
      tr->announce[tk->token_size] = '\0';
      /* ------------------------ */
    } else if (tk->token_size == strlen("comment") &&
               memcmp(tk->token, "comment", strlen("comment")) == 0) {
      /* --- parse comment --- */
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      if (memcmp(tk->token, "s", tk->token_size) != 0) {
        fprintf(stderr, "expected s token, got %.*s\n", (int)tk->token_size, tk->token);
        ok = false;
        break;
      }
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      tr->comment = realloc(tr->comment, tk->token_size + 1);
      memcpy(tr->comment, tk->token, tk->token_size);
      tr->comment[tk->token_size] = '\0';
      /* ------------------------ */
    } else if (tk->token_size == strlen("info") &&
               memcmp(tk->token, "info", strlen("info")) == 0) {
      info_start = tk->current;
      info_start_stack_value = tk->list_dict_stack;
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      if (memcmp(tk->token, "d", tk->token_size) != 0) {
        fprintf(stderr, "expected d token, got %.*s\n", (int)tk->token_size, tk->token);
        ok = false;
        break;
      }
    } else if (info_start != NULL && info_end == NULL &&
               info_start_stack_value == tk->list_dict_stack && tk->token_size == strlen("e") &&
               memcmp(tk->token, "e", strlen("e")) == 0) {
      info_end = tk->current - 1;  // the tk->current pointer is one char over the 'e' char
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      tr->info_hash = malloc(BT_HASH_LENGTH);
      perform_hash(info_start, info_end - info_start + 1, tr->info_hash);
    } else if (tk->token_size == strlen("name") &&
               memcmp(tk->token, "name", strlen("name")) == 0) {
      /* --- parse name --- */
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      if (memcmp(tk->token, "s", tk->token_size) != 0) {
        fprintf(stderr, "expected s token, got %.*s\n", (int)tk->token_size, tk->token);
        ok = false;
        break;
      }
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      tr->name = realloc(tr->name, tk->token_size + 1);
      memcpy(tr->name, tk->token, tk->token_size);
      tr->name[tk->token_size] = '\0';
      /* ------------------------ */
    } else if (tk->token_size == strlen("length") &&
               memcmp(tk->token, "length", strlen("length")) == 0) {
      /* --- parse length --- */
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      if (memcmp(tk->token, "i", tk->token_size) != 0) {
        fprintf(stderr, "expected i token, got %.*s\n", (int)tk->token_size, tk->token);
        ok = false;
        break;
      }
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      char *num = malloc(tk->token_size + 1);
      memcpy(num, tk->token, tk->token_size);
      num[tk->token_size] = '\0';
      tr->length = strtoll(num, NULL, 10);
      free(num);
      /* ------------------------ */
    } else if (tk->token_size == strlen("piece length") &&
               memcmp(tk->token, "piece length", strlen("piece length")) == 0) {
      /* --- parse piece length --- */
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      if (memcmp(tk->token, "i", tk->token_size) != 0) {
        fprintf(stderr, "expected i token, got %.*s\n", (int)tk->token_size, tk->token);
        ok = false;
        break;
      }
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      char *num = malloc(tk->token_size + 1);
      memcpy(num, tk->token, tk->token_size);
      num[tk->token_size] = '\0';
      tr->piece_length = strtoll(num, NULL, 10);
      free(num);
      /* ------------------------ */
    } else if (tk->token_size == strlen("pieces") &&
               memcmp(tk->token, "pieces", strlen("pieces")) == 0) {
      /* --- parse pieces --- */
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      if (memcmp(tk->token, "s", tk->token_size) != 0) {
        fprintf(stderr, "expected s token, got %.*s\n", (int)tk->token_size, tk->token);
        ok = false;
        break;
      }
      err = next(tk);
      if (err != TOKENIZER_OK) {
        fprintf(stderr, "tokenizer failed with code %d\n", err);
        ok = false;
        break;
      }
      tr->num_pieces = tk->token_size / BT_HASH_LENGTH;
      tr->piece_hashes = malloc(tr->num_pieces * sizeof(char *));
      for (long long i = 0; i < tr->num_pieces; i++) {
        tr->piece_hashes[i] = malloc(BT_HASH_LENGTH);
        memcpy(tr->piece_hashes[i], tk->token + BT_HASH_LENGTH * i, BT_HASH_LENGTH);
      }
      /* ------------------------ */
    }
  }

  free_tokenizer(tk);
  fprintf(stdout, "parsing done\n");
  return ok;
}

void free_torrent_file(torrent_file *t)
{
  free(t->name);
  if (t->piece_hashes != NULL) {
    for (long long i = 0; i < t->num_pieces; i++) {
      free(t->piece_hashes[i]);
    }
  }
  free(t->piece_hashes);
  free(t->info_hash);
  free(t->comment);
  free(t->announce);
  free(t);
}
