#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cout << "could not create the socket " << std::endl;
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    std::cout << "Could not bind to the port \n";
    return -1;
  }

  char msg[] = "hello";
  write(fd, msg, sizeof(msg));

  char rbuff[64] = {};
  ssize_t n = read(fd, rbuff, sizeof(rbuff) - 1);
  if (n < 0) {
    std::cout << "Error in read\n";
  }

  std::cout << "Server Response: " << rbuff << std::endl;
  close(fd);

  return 0;
}
