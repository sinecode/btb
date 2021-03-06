#include "client.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "big_endian.h"
#include "conn.h"
#include "defer.h"
#include "downloaded_file.h"
#include "handshake_msg.h"
#include "message.h"
#include "peer.h"
#include "peer_id.h"
#include "piece_progress.h"
#include "pieces_pool.h"
#include "torrent_file.h"
#include "tracker_response.h"

#define TIMEOUT_SEC 5
// Each peer, which corresponds to a thread
// will be aborted after PEER_MAX_FAILS failed attempt to download a piece.
#define PEER_MAX_FAILS 8

static bool download_piece(piece_progress *p_prog, size_t piece_size, conn *c, torrent_file *tf)
{
  size_t num_blocks = p_prog->size / BLOCK_SIZE + (p_prog->size % BLOCK_SIZE);
  fprintf(stdout, "\n");
  fprintf(stdout, "about to download piece index=%lu in %lu blocks\n", p_prog->index, num_blocks);
  while (p_prog->downloaded < num_blocks) {
    /******* Request *******/
    unsigned char request_msg_payload[PIECE_INDEX_BYTES + BLOCK_OFFSET_BYTES + BLOCK_LENGTH_BYTES];
    bool ok = lu_to_big_endian(p_prog->index, request_msg_payload, PIECE_INDEX_BYTES);
    if (!ok) {
      fprintf(stderr, "lu_to_big_endian failed\n");
      return false;
    }
    size_t block_offset = p_prog->downloaded * BLOCK_SIZE;
    ok = lu_to_big_endian(block_offset, request_msg_payload + PIECE_INDEX_BYTES,
                          BLOCK_OFFSET_BYTES);
    if (!ok) {
      fprintf(stderr, "lu_to_big_enian failed\n");
      return false;
    }
    ok = lu_to_big_endian(BLOCK_SIZE, request_msg_payload + PIECE_INDEX_BYTES + BLOCK_OFFSET_BYTES,
                          BLOCK_LENGTH_BYTES);
    if (!ok) {
      fprintf(stderr, "lu_to_big_endian failed\n");
      return false;
    }
    message *msg_request =
        init_message(MSG_ID_REQUEST, PIECE_INDEX_BYTES + BLOCK_OFFSET_BYTES + BLOCK_LENGTH_BYTES,
                     (char *)request_msg_payload);
    DEFER({ free_message(msg_request); });
    fprintf(stdout, "sending \"Request\" message for a block with offset %lu of piece #%lu\n",
            block_offset, p_prog->index);
    ok = send_message_to_conn(msg_request, c, TIMEOUT_SEC);
    if (!ok) {
      fprintf(stderr, "send_message_to_conn failed\n");
      return false;
    }
    fprintf(stdout, "\"Request\" message sent\n");
    /*********************/

    /******* Piece *******/
    fprintf(stdout, "Reading \"Piece\" message\n");
    message *recv_piece_msg = read_message_from_conn(c, TIMEOUT_SEC);
    if (recv_piece_msg == NULL) {
      fprintf(stdout, "read_message_from_conn failed\n");
      return false;
    }
    DEFER({ free_message(recv_piece_msg); });
    if (recv_piece_msg->id != MSG_ID_PIECE) {
      fprintf(stdout, "expected \"Piece\" message, got message ID %hu\n", recv_piece_msg->id);
      return false;
    }
    size_t got_piece_index =
        big_endian_to_lu((unsigned char *)recv_piece_msg->payload, PIECE_INDEX_BYTES);
    size_t got_block_offset = big_endian_to_lu(
        (unsigned char *)recv_piece_msg->payload + PIECE_INDEX_BYTES, BLOCK_OFFSET_BYTES);
    size_t got_block_size = recv_piece_msg->payload_len - PIECE_INDEX_BYTES - BLOCK_OFFSET_BYTES;
    fprintf(stdout,
            "got \"Piece\" message with block size=%lu, block offset=%lu and piece index=%lu\n",
            got_block_size, got_block_offset, got_piece_index);
    if (p_prog->index != got_piece_index) {
      fprintf(stderr, "wrong block offset: got %lu, want %lu\n", got_piece_index, p_prog->index);
      return false;
    }
    if (block_offset != got_block_offset) {
      fprintf(stderr, "wrong block offset: got %lu, want %lu\n", got_block_offset, block_offset);
      return false;
    }
    memcpy(p_prog->buf + got_block_offset,
           recv_piece_msg->payload + PIECE_INDEX_BYTES + BLOCK_OFFSET_BYTES, got_block_size);
    p_prog->downloaded++;
    fprintf(stdout, "downloaded block %lu/%lu of piece index=%lu\n", p_prog->downloaded,
            num_blocks, p_prog->index);
    fprintf(stdout, "\n");
    /*********************/
  }
  return true;
}

typedef struct thread_data
{
  peer *p;
  torrent_file *tf;
  pieces_pool *pool;
  downloaded_file *df;
  bool ok;
} thread_data;

static thread_data *init_thread_data(peer *p, torrent_file *tf, pieces_pool *pool,
                                     downloaded_file *df)
{
  thread_data *d = malloc(sizeof(thread_data));
  d->p = p;
  d->tf = tf;
  d->pool = pool;
  d->df = df;
  d->ok = false;
  return d;
}

static void free_thread_data(thread_data *d)
{
  if (d == NULL) return;
  free(d);
}

static void *download_torrent_thread_fun(void *data)
{
  thread_data *d = (thread_data *)data;
  d->ok = false;
  fprintf(stdout, "\n");
  char peer_addr[15 + 1];  // An address is of the form xxx.xxx.xxx.xxx + '\0'
  sprintf(peer_addr, "%d.%d.%d.%d", d->p->address[0], d->p->address[1], d->p->address[2],
          d->p->address[3]);
  conn *c = init_conn(peer_addr, d->p->port, TIMEOUT_SEC);
  DEFER({ free_conn(c); });
  if (c == NULL) {
    fprintf(stderr, "init_conn failed\n");
    return NULL;
  }

  /***** HANDSHAKE *****/
  handshake_msg *hm = init_handshake_msg(d->tf->info_hash, MY_PEER_ID);
  DEFER({ free_handshake_msg(hm); });
  handshake_msg_encoded *hm_encoded = encode_handshake_msg(hm);
  DEFER({ free_handshake_msg_encoded(hm_encoded); });
  fprintf(stdout, "performing handshake with peer %s:%hu\n", peer_addr, d->p->port);
  bool ok = send_data(c, hm_encoded->buf, hm_encoded->size, TIMEOUT_SEC);
  if (!ok) {
    fprintf(stderr, "send_data failed\n");
    return NULL;
  }
  char *hm_buf = malloc(hm_encoded->size);
  DEFER({ free(hm_buf); });
  int bytes_receive = receive_data(c, hm_buf, hm_encoded->size, TIMEOUT_SEC);
  if (bytes_receive <= 0) {
    fprintf(stderr, "receive_data failed\n");
    return NULL;
  }
  fprintf(stdout, "performed handshake with peer %s:%hu\n", peer_addr, d->p->port);
  handshake_msg *hm_reply = decode_handshake_msg(hm_buf, hm_encoded->size);
  DEFER({ free_handshake_msg(hm_reply); });
  if (hm_reply == NULL) {
    fprintf(stderr, "decode_handshake_msg failed\n");
    return NULL;
  }
  if (memcmp(hm_reply->info_hash, hm->info_hash, BT_HASH_LENGTH) != 0) {
    fprintf(stderr, "info hashes don't match, aborting connection with peer\n");
    return NULL;
  }
  /**********************/

  // NOTE: the bitfield is optional, some peers could send "HAVE" messages.
  // For now, we assume that after the handshake, the bitfield message is sent.
  /***** BITFIELD ******/
  size_t bitfield_msg_size = ceil(d->tf->num_pieces / 8.0) + MSG_ID_BYTES + MSG_LEN_BYTES;
  char *bitfield_buf = malloc(bitfield_msg_size);
  DEFER({ free(bitfield_buf); });
  fprintf(stdout, "receving bitfield from peer %s:%hu\n", peer_addr, d->p->port);
  message *bitfield_msg = read_message_from_conn(c, TIMEOUT_SEC);
  if (bitfield_msg == NULL) {
    fprintf(stderr, "decode_message failed\n");
    return NULL;
  }
  DEFER({ free_message(bitfield_msg); });
  if (bitfield_msg->id != MSG_ID_BITFIELD) {
    fprintf(stderr, "expected \"Bitfield\" message, got message ID=%hu\n", bitfield_msg->id);
    return NULL;
  }
  fprintf(stdout, "received bitifield from peer %s:%hu\n", peer_addr, d->p->port);
  bitfield *peer_bf =
      init_bitfield((unsigned char *)bitfield_msg->payload, bitfield_msg->payload_len);
  DEFER({ free_bitfield(peer_bf); });
  /*********************/

  /**** INTERESTED *****/
  message *interested_msg = init_message(MSG_ID_INTERESTED, 0, NULL);
  DEFER({ free_message(interested_msg); });
  fprintf(stdout, "sending \"Interested\" message to peer %s:%hu\n", peer_addr, d->p->port);
  ok = send_message_to_conn(interested_msg, c, TIMEOUT_SEC);
  if (!ok) {
    fprintf(stderr, "send_data failed\n");
    return NULL;
  }
  fprintf(stdout, "sent \"Interested\" message to peer %s:%hu\n", peer_addr, d->p->port);
  /*********************/

  /******* State *******/
  message *recv_msg = read_message_from_conn(c, TIMEOUT_SEC);
  if (recv_msg == NULL) {
    fprintf(stderr, "read_message_from_conn failed\n");
    return NULL;
  }
  DEFER({ free_message(recv_msg); });
  switch (recv_msg->id) {
    case MSG_ID_CHOKE:
      fprintf(stdout, "received a \"Choke\" message\n");
      return NULL;
    case MSG_ID_UNCHOKE:
      fprintf(stdout, "received a \"Unchoke\" message\n");

      /*** Try to download pieces ***/

      int failed_attempts = 0;
      while (!is_done(d->pool) && failed_attempts < PEER_MAX_FAILS) {
        size_t piece_index = get_piece_index(d->pool);
        if (!has_piece(peer_bf, piece_index)) {
          fprintf(stdout, "peer %s:%hu doesn't have piece #%lu\n", peer_addr, d->p->port,
                  piece_index);
          mark_as_undone(d->pool, piece_index);
          failed_attempts++;
          continue;  // Try to download another piece
        }
        fprintf(stdout, "peer %s:%hu has piece #%lu\n", peer_addr, d->p->port, piece_index);
        piece_progress *p_prog = init_piece_progress(piece_index, d->tf->piece_length);
        DEFER({ free_piece_progress(p_prog); });
        ok = download_piece(p_prog, d->tf->piece_length, c, d->tf);
        if (!ok) {
          mark_as_undone(d->pool, piece_index);
          failed_attempts++;
          continue;  // Try to download another piece
        }
        fprintf(stdout, "peer %s:%hu succeeded to download piece #%lu\n", peer_addr, d->p->port,
                piece_index);
        add_piece(d->df, p_prog);
      }
      break;  // Success

      /*********************************/

    case MSG_ID_INTERESTED:
      fprintf(stdout, "received a \"Interested\" message\n");
      return NULL;
    case MSG_ID_REQUEST:
      fprintf(stdout, "received a \"Request\" message\n");
      return NULL;
    case MSG_ID_PIECE:
      fprintf(stdout, "received a \"Piece\" message\n");
      return NULL;
    case MSG_ID_CANCEL:
      fprintf(stdout, "received a \"Cancel\" message\n");
      return NULL;
    default:
      fprintf(stdout, "Unknown or useless message ID: %hu\n", recv_msg->id);
      return NULL;
  }
  /*********************/
  d->ok = true;  // Success
  return NULL;
}

bool download_torrent(const char *torrent_fname)
{
  torrent_file *tf = init_torrent_file();
  DEFER({ free_torrent_file(tf); });
  bool ok = parse_torrent_file(tf, torrent_fname);
  if (!ok) {
    fprintf(stderr, "parse_torrent_file failed\n");
    return false;
  }
  fprintf(stdout,
          "the file is %lu bytes total and it is divided in %lu pieces of %lu bytes each\n",
          tf->length, tf->num_pieces, tf->piece_length);
  tracker_response *tr = init_tracker_response();
  DEFER({ free_tracker_response(tr); });
  fprintf(stdout, "contacting tracker at %s\n", tf->announce);
  ok = contact_tracker(tr, tf);
  if (!ok) {
    fprintf(stderr, "contact_tracker failed\n");
    return false;
  }
  fprintf(stdout, "tracker replied with the addresses of %lu peers\n", tr->num_peers);

  pieces_pool *pp = init_pieces_pool(tf->num_pieces);
  DEFER({ free_pieces_pool(pp); });

  // Buffer that will hold the pieces when they have been downloaded.
  downloaded_file *df = init_downloaded_file(tf);
  DEFER({ free_downloaded_file(df); });

  // Each peer corresponds to a thread
  size_t num_threads = tr->num_peers;
  // size_t num_threads = 1;
  thread_data *data[num_threads];
  pthread_t thread_ids[num_threads];

  for (size_t i = 0; i < num_threads; i++) {
    data[i] = init_thread_data(tr->peers[i], tf, pp, df);
    pthread_create(&thread_ids[i], NULL, download_torrent_thread_fun, data[i]);
  }

  for (size_t i = 0; i < num_threads; i++) {
    pthread_join(thread_ids[i], NULL);
    free_thread_data(data[i]);
  }

  if (!is_done(pp)) {
    fprintf(stderr, "There are %lu pieces to be downloaded\n", get_num_undone_pieces(pp));
    return false;
  } else {
    fprintf(stdout, "All %lu pieces have been downloaded\n", tf->num_pieces);
  }

  ok = write_to_file(df);
  if (!ok) {
    fprintf(stderr, "write_to_file failed\n");
    return false;
  }
  fprintf(stdout, "file successfully downloaded in %s\n", tf->name);

  return true;
}