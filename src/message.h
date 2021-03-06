#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "conn.h"

// Number of bytes to represent the message length.
#define MSG_LEN_BYTES 4
// Number of bytes to represent the message ID.
#define MSG_ID_BYTES 1

#define MSG_ID_CHOKE 0
#define MSG_ID_UNCHOKE 1
#define MSG_ID_INTERESTED 2
#define MSG_ID_NOTINTERESTED 3
#define MSG_ID_HAVE 4
#define MSG_ID_BITFIELD 5
#define MSG_ID_REQUEST 6
#define MSG_ID_PIECE 7
#define MSG_ID_CANCEL 8

typedef struct message
{
  uint8_t id;
  size_t payload_len;
  char *payload;
} message;

message *init_message(uint8_t msg_id, size_t payload_len, const char *payload);

void free_message(message *);

typedef struct message_encoded
{
  size_t size;
  char *buf;
} message_encoded;

message_encoded *encode_message(message *);

void free_message_encoded(message_encoded *);

// Decode a message from buf.
// It returns NULL if it fails.
message *decode_message(char *buf, size_t buf_length);

// Send a message in the input connection.
// It returns false if it fails.
bool send_message_to_conn(message *msg, conn *c, int timeout_sec);

// Read a message from the input connection.
// It returns NULL if it fails.
message *read_message_from_conn(conn *c, int timeout_sec);
