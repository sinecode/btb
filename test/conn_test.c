#include "conn_test.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../src/conn.h"
#include "../src/defer.h"

void test_init_conn()
{
  conn* c = init_conn("www.example.com", 80, 10);
  DEFER({ free_conn(c); });
  if (c == NULL) {
    fprintf(stderr, "init_conn failed\n");
  }
}

void test_init_conn_fail()
{
  conn* c = init_conn("unknown-address", 9999, 10);
  DEFER({ free_conn(c); });
  if (c != NULL) {
    fprintf(stderr, "init_conn should fail\n");
  }
}

void test_send_data()
{
  conn* c = init_conn("www.example.com", 80, 10);
  DEFER({ free_conn(c); });
  if (c == NULL) {
    fprintf(stderr, "init_conn failed\n");
    return;
  }
  char buf[128] = "some data to send";
  bool ok = send_data(c, buf, strlen(buf), 10);
  if (!ok) {
    fprintf(stderr, "send_data failed\n");
    return;
  }
}

void test_receive_data_with_timeout()
{
  conn* c = init_conn("www.example.com", 80, 10);
  DEFER({ free_conn(c); });
  if (c == NULL) {
    fprintf(stderr, "init_conn failed\n");
    return;
  }
  char buf[128];
  int timeout_sec = 1;
  int bytes_received = receive_data(c, buf, 128, timeout_sec);
  if (bytes_received > 0) {
    fprintf(stderr, "receive_data should fail because timeout\n");
    return;
  }
}