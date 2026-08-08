
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

static void do_something(int connfd) {
  char rbuf[64] = {};
  ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
  if (n < 0) {
    std::cout << "Read() error";
    return;
  }

  cout << "Client: " << rbuf << endl;

  char wbuf[] = "world";
  write(connfd, wbuf, strlen(wbuf));
}

int main() {
  std::cout << "Server \n";

  int fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET is for ipv4
  if (fd < 0) {
    std::cout << "could not create the socket" << endl;
  }

  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
             sizeof(val)); // set socket options

  // Bind socket port
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohs(0); // Wild chard - 0.0.0.0
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    std::cout << "could not bind socket address" << endl;
  }

  // start listening to port 1234
  rv = listen(fd, SOMAXCONN);
  if (rv) {
    std::cout << "could not start listening to the ports" << endl;
  }

  std::cout << "Listening to port: " << addr.sin_port << endl;
  while (true) {

    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
      continue;
    } else {
      std::cout << "Client Connected: " << client_addr.sin_port << endl;
    }

    do_something(connfd);
    close(connfd);
  }
}
