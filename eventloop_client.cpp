
#include <arpa/inet.h>
#include <assert.h>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>
#include <vector>

using namespace std;

static int32_t read_full(int fd, uint8_t *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1; // error, or unexpected EOF
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

static int32_t write_all(int fd, const uint8_t *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = write(fd, buf, n);
    if (rv <= 0) {
      return -1; // error
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

// append to the back
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data,
                       size_t len) {
  buf.insert(buf.end(), data, data + len);
}

const size_t k_max_msg = 32 << 20; // likely larger than the kernel buffer

// the `query` function was simply splited into `send_req` and `read_res`.
static int32_t send_req(int fd, const uint8_t *text, size_t len) {
  if (len > k_max_msg) {
    return -1;
  }

  std::vector<uint8_t> wbuf;
  buf_append(wbuf, (const uint8_t *)&len, 4);
  buf_append(wbuf, text, len);
  return write_all(fd, wbuf.data(), wbuf.size());
}

static int32_t read_res(int fd) {
  std::vector<uint8_t> rbuf;
  rbuf.resize(4);

  errno = 0;

  int32_t err = read_full(fd, &rbuf[0], 4);
  if (err) {
    if (errno == 0) {
      cout << "EOF" << endl;
    } else {
      cout << "Error" << endl;
    }

    return err;
  }

  uint32_t len;
  memcpy(&len, rbuf.data(), 4);
  if (len > k_max_msg) {
    cout << "too long" << endl;
    return -1;
  }

  rbuf.resize(4 + len);
  err = read_full(fd, &rbuf[4], len);
  if (err) {
    cout << "read() error" << endl;
    return err;
  }

  printf("len:%u data:%.*s\n", len, len < 100 ? len : 100, &rbuf[4]);

  return 0;
}

int main() {

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    cout << "socket()" << endl;
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1
  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    cout << "connect" << endl;
  }
  // multiple pipelined requests
  std::vector<std::string> query_list = {
      "hello1",
      "hello2",
      "hello3",
      // a large message requires multiple event loop iterations
      std::string(k_max_msg, 'z'),
      "hello5",
  };
  for (const std::string &s : query_list) {
    int32_t err = send_req(fd, (uint8_t *)s.data(), s.size());
    if (err) {
      goto L_DONE;
    }
  }
  for (size_t i = 0; i < query_list.size(); ++i) {
    int32_t err = read_res(fd);
    if (err) {
      goto L_DONE;
    }
  }

L_DONE:
  close(fd);
  return 0;
}
