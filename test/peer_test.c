#include "peer_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/defer.h"
#include "../src/peer.h"

void test_peer()
{
  unsigned char peer_repr[PEER_BLOB_SIZE] = {
      124, 0, 100, 105,  // address
      1,   1,            // port
  };
  peer *p = init_peer(peer_repr);
  DEFER({ free_peer(p); });
  char wantAddr[] = "124.0.100.105";
  char *gotAddr = malloc(strlen(wantAddr) + 1);
  DEFER({ free(gotAddr); });
  sprintf(gotAddr, "%d.%d.%d.%d", p->address[0], p->address[1], p->address[2], p->address[3]);
  if (strcmp(gotAddr, wantAddr) != 0) {
    fprintf(stderr, "wrong peer address: got %s, want %s\n", gotAddr, wantAddr);
  }
  if (p->port != 257) {
    fprintf(stderr, "wrong peer port: got %d, want %d\n", p->port, 257);
  }
}
