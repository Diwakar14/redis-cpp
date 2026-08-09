#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

const size_t max_msg_size = 4096;

static int32_t read_full(int fd, char *rbuf, size_t len) {

  while (len > 0) {
    ssize_t rv = read(fd, rbuf, len);
    if (rv <= 0) {
      return -1;
    }

    assert((size_t)rv <= len);
    len -= (size_t)rv;
    rbuf += rv;
  }

  return 0;
}

static int32_t write_full(int fd, const char *wbuf, size_t len) {

  while (len > 0) {
    ssize_t rv = write(fd, wbuf, len);
    if (rv <= 0) {
      return -1;
    }

    len -= (size_t)rv;
    wbuf += rv;
  }

  return 0;
}

int32_t query(int fd, const char *q) {
  uint32_t len = (uint32_t)strlen(q);

  cout << "query: " << q << ",len: " << len << endl;

  if (len > max_msg_size) {
    cout << "message too long" << endl;
    return -1;
  }

  // Send message to the server
  char wbuff[4 + max_msg_size];
  memcpy(wbuff, &len, 4);
  memcpy(&wbuff[4], q, len);

  cout << "wbuff -> " << wbuff << " " << q << " " << len << endl;
  if (int32_t err = write_full(fd, wbuff, 4 + len)) {
    cout << "error writing to the stream" << endl;
    return err;
  }
  // Receive message from the server.
  // Read the 4byte header
  char rbuff[4 + max_msg_size];
  errno = 0;
  int32_t err = read_full(fd, rbuff, 4);
  if (err != 0) {
    cout << "error reading the header..." << endl;
    return -1;
  }

  memcpy(&len, rbuff, 4);
  if (len > max_msg_size) {
    cout << "message too long" << endl;
    return -1;
  }

  // Read the body
  err = read_full(fd, &rbuff[4], len);
  if (err != 0) {
    cout << "error reading the request body" << endl;
    return -1;
  }

  cout << "Respone from server: " << &rbuff[4] << endl;

  return 0;
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd <= 0) {
    cout << "could not create the socket" << endl;
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    cout << "could not connect to the server" << endl;
  }

  int32_t err = query(fd, "hello1");
  if (err) {
    goto L_DONE;
  }

  // err = query(fd, "hello2");
  // if (err) {
  //   goto L_DONE;
  // }
  //
  // err = query(fd, "hello3");
  // if (err) {
  //   goto L_DONE;
  // }

L_DONE:
  close(fd);
  return 0;
}
