#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

static int32_t read_full(int connfd, char *rbuff, size_t n) {
  while (n > 0) {
    ssize_t rv = read(connfd, rbuff, n);
    if (rv <= 0) {
      return -1;
    }

    n -= (size_t)rv;
    rbuff += rv;
  }

  return 0;
}

static int32_t write_full(int connfd, const char *rbuff, size_t n) {
  while (n > 0) {
    ssize_t rv = write(connfd, rbuff, n);
    if (rv <= 0) {
      return -1;
    }

    n -= (size_t)rv;
    rbuff += rv;
  }

  return 0;
}

const size_t max_msg = 4096;
int32_t one_request(int connfd) {

  cout << "-- Req Called --\n";
  // Simple Protocol: header|content|header|content...
  // 1. Read the header
  char rbuff[4 + max_msg];
  errno = 0;
  int32_t err = read_full(connfd, rbuff, 4);
  if (err != 0) {
    cout << "read failed - " << err << endl;
    return -1;
  }

  // Get the length of the header
  cout << "Buffer - " << &rbuff << endl;
  uint32_t len = 0;
  memcpy(&len, rbuff, 4);
  if (len > max_msg) {
    cout << "Message too long..." << len << endl;
    return -1;
  }

  // Read the request payload
  err = read_full(connfd, &rbuff[4], len);
  if (err != 0) {
    cout << "could not read from stream..." << err << endl;
    return -1;
  }

  cout << "Client Req: " << &rbuff[4] << endl;

  const char response[] = "Server got the request";
  char wbuff[4 + sizeof(response)];
  len = (uint32_t)strlen(response);

  memcpy(wbuff, &len, 4);
  memcpy(&wbuff[4], response, len);

  return write_full(connfd, wbuff, 4 + len);
}

int main() {

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    cout << "could not create socket " << endl;
  }

  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = ntohs(0); // wild card
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    cout << "could not bind to the port..." << endl;
  }

  rv = listen(fd, SOMAXCONN);
  if (rv) {
    cout << "could not listen to the specified port " << endl;
  }

  cout << "Server listening on port: " << addr.sin_port << endl;
  while (true) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);

    if (connfd < 0) {
      continue;
    } else {
      cout << "Client Connected: " << client_addr.sin_port << endl;
    }

    while (true) {
      int32_t erro = one_request(connfd);
      if (erro < 0) {
        break;
      }
    }
    // close(connfd);
    // Do Something...
    close(fd);
  }
};
