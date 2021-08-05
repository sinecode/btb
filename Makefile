TARGET := btb
SRC_DIR := src
TARGET_TEST := btb-test
TEST_DIR := test

CC := gcc
WARNINGS := -Wall -Werror
CSTD := c17
CFLAGS_ALL := -std=$(CSTD) $(WARNINGS)
CFLAGS_TEST := -g
CFLAGS_RELEASE := -O3
LIBS := -lcrypto -lcurl -lpthread -lm
VALGRIND_SUPP_FILE := valgrind.supp
VALGRIND_CMD := valgrind --leak-check=full --show-leak-kinds=all --suppressions=$(VALGRIND_SUPP_FILE)

.PHONY: clean test

all: test build

build: \
	hash.o \
	tokenizer.o \
	file_buf.o \
	torrent_file.o \
	peer.o \
	tracker_response.o \
	handshake_msg.o \
	message.o \
	bitfield.o \
	pieces_pool.o \
	piece_progress.o \
	big_endian.o \
	conn.o \
	client.o \
	$(SRC_DIR)/main.c

	$(CC) $(CFLAGS_ALL) $(CFLAGS_RELEASE) -o $(TARGET) $? $(LIBS)

test: \
	hash.o \
	tokenizer.o tokenizer_test.o \
	file_buf.o file_buf_test.o \
	torrent_file.o torrent_file_test.o \
	peer.o peer_test.o \
	tracker_response.o \
	handshake_msg.o handshake_msg_test.o \
	message.o message_test.o \
	bitfield.o bitfield_test.o \
	pieces_pool.o pieces_pool_test.o \
	piece_progress.o \
	big_endian.o big_endian_test.o \
	conn.o conn_test.o \
	client.o client_test.o \
	$(TEST_DIR)/main.c

	$(CC) $(CFLAGS_ALL) $(CFLAGS_TEST) -o $(TARGET_TEST) $? $(LIBS)
	$(VALGRIND_CMD) ./$(TARGET_TEST)

client_test.o: $(TEST_DIR)/client_test.c
	@ $(CC) $(CFLAGS) -c $?

client.o: $(SRC_DIR)/client.c
	@ $(CC) $(CFLAGS) -c $?

conn_test.o: $(TEST_DIR)/conn_test.c
	@ $(CC) $(CFLAGS) -c $?

conn.o: $(SRC_DIR)/conn.c
	@ $(CC) $(CFLAGS) -c $?

piece_progress.o: $(SRC_DIR)/piece_progress.c
	@ $(CC) $(CFLAGS) -c $?

pieces_pool_test.o: $(TEST_DIR)/pieces_pool_test.c
	@ $(CC) $(CFLAGS) -c $?

pieces_pool.o: $(SRC_DIR)/pieces_pool.c
	@ $(CC) $(CFLAGS) -c $?

bitfield_test.o: $(TEST_DIR)/bitfield_test.c
	@ $(CC) $(CFLAGS) -c $?

bitfield.o: $(SRC_DIR)/bitfield.c
	@ $(CC) $(CFLAGS) -c $?

message_test.o: $(TEST_DIR)/message_test.c
	@ $(CC) $(CFLAGS) -c $?

message.o: $(SRC_DIR)/message.c
	@ $(CC) $(CFLAGS) -c $?

handshake_msg_test.o: $(TEST_DIR)/handshake_msg_test.c
	@ $(CC) $(CFLAGS) -c $?

handshake_msg.o: $(SRC_DIR)/handshake_msg.c
	@ $(CC) $(CFLAGS) -c $?

tracker_response.o: $(SRC_DIR)/tracker_response.c
	@ $(CC) $(CFLAGS) -c $?

peer_test.o: $(TEST_DIR)/peer_test.c
	@ $(CC) $(CFLAGS) -c $?

peer.o: $(SRC_DIR)/peer.c
	@ $(CC) $(CFLAGS) -c $?

torrent_file_test.o: $(TEST_DIR)/torrent_file_test.c
	@ $(CC) $(CFLAGS) -c $?

torrent_file.o: $(SRC_DIR)/torrent_file.c
	@ $(CC) $(CFLAGS) -c $?

big_endian_test.o: $(TEST_DIR)/big_endian_test.c
	@ $(CC) $(CFLAGS) -c $?

big_endian.o: $(SRC_DIR)/big_endian.c
	@ $(CC) $(CFLAGS) -c $?

file_buf_test.o: $(TEST_DIR)/file_buf_test.c
	@ $(CC) $(CFLAGS) -c $?

file_buf.o: $(SRC_DIR)/file_buf.c
	@ $(CC) $(CFLAGS) -c $?

tokenizer_test.o: $(TEST_DIR)/tokenizer_test.c
	@ $(CC) $(CFLAGS) -c $?

tokenizer.o: $(SRC_DIR)/tokenizer.c
	@ $(CC) $(CFLAGS) -c $?

hash.o: $(SRC_DIR)/hash.c
	@ $(CC) $(CFLAGS) -c $?

clean:
	@ $(RM) $(TARGET) $(TARGET_TEST) *.o
