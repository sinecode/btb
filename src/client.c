#include "client.h"

#include <stdio.h>
#include <string.h>

#include "conn.h"
#include "handshake_msg.h"
#include "message.h"
#include "peer.h"
#include "peer_id.h"
#include "pieces_pool.h"
#include "torrent_file.h"
#include "tracker_response.h"

#define TIMEOUT_SEC 10

bool download_torrent(const char *filename)
{
  bool ok = false;
  torrent_file *tf = init_torrent_file();
  ok = parse_torrent_file(tf, filename);
  if (!ok) {
    fprintf(stderr, "parse_torrent_file failed\n");
    goto exit_0;
  }
  tracker_response *tr = init_tracker_response();
  fprintf(stdout, "contacting tracker at %s\n", tf->announce);
  ok = contact_tracker(tr, tf);
  if (!ok) {
    fprintf(stderr, "contact_tracker failed\n");
    goto exit_1;
  }
  fprintf(stdout, "tracker replied with the addresses of %lu peers\n", tr->num_peers);

  pieces_pool *pp = init_pieces_pool(tf->num_pieces);
  // size_t piece_index = get_piece_index(pp);
  for (size_t i = 0; i < tr->num_peers; i++) {
    peer *p = tr->peers[i];
    char peer_addr[15 + 1];  // An address is of the form xxx.xxx.xxx.xxx + '\0'
    sprintf(peer_addr, "%d.%d.%d.%d", p->address[0], p->address[1], p->address[2], p->address[3]);
    // TODO: start a new thread here with timedjoin
    conn *c = init_conn(peer_addr, p->port, TIMEOUT_SEC);
    if (c == NULL) {
      fprintf(stderr, "init_conn failed\n");
      continue;
    }
    handshake_msg *hm = init_handshake_msg(tf->info_hash, MY_PEER_ID);
    handshake_msg_encoded *hm_encoded = encode_handshake_msg(hm);
    fprintf(stdout, "perfoming handshake with peer %s:%hu\n", peer_addr, p->port);
    ok = send_data(c, hm_encoded->buf, hm_encoded->size, TIMEOUT_SEC);
    if (!ok) {
      fprintf(stderr, "send_data failed\n");
      free_handshake_msg_encoded(hm_encoded);
      free_handshake_msg(hm);
      free_conn(c);
      continue;
    }
    char *hm_buf = malloc(hm_encoded->size);
    ok = receive_data(c, hm_buf, hm_encoded->size, TIMEOUT_SEC);
    if (!ok) {
      fprintf(stderr, "receive_data failed\n");
      free(hm_buf);
      free_handshake_msg_encoded(hm_encoded);
      free_handshake_msg(hm);
      free_conn(c);
      continue;
    }
    fprintf(stdout, "performed handshake with peer %s:%hu\n", peer_addr, p->port);
    handshake_msg *hm_reply = decode_handshake_msg(hm_buf, hm_encoded->size);
    free(hm_buf);
    if (hm_reply == NULL) {
      fprintf(stderr, "decode_handshake_msg failed\n");
      free_handshake_msg_encoded(hm_encoded);
      free_handshake_msg(hm);
      free_conn(c);
      continue;
    }
    if (memcmp(hm_reply->info_hash, hm->info_hash, BT_HASH_LENGTH) != 0) {
      fprintf(stderr, "info hashes don't match, aborting connection with peer\n");
      free_handshake_msg(hm_reply);
      free_handshake_msg_encoded(hm_encoded);
      free_handshake_msg(hm);
      free_conn(c);
      continue;
    }

    free_handshake_msg(hm_reply);
    free_handshake_msg_encoded(hm_encoded);
    free_handshake_msg(hm);
    free_conn(c);
    break;
  }

  // exit_2:
  free_pieces_pool(pp);
exit_1:
  free_tracker_response(tr);
exit_0:
  free_torrent_file(tf);
  return ok;
}