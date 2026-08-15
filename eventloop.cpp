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

struct Conn {

  int fd = -1;
  bool want_read = false;
  bool want_write = false;
  bool want_close = false;

  std::vector<uint8_t> incoming;
  std::vector<uint8_t> outgoing;
};

static void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    return;
  }

  flags |= O_NONBLOCK;

  errno = 0;
  (void)fcntl(fd, F_SETFL, flags);
  if (errno) {
    return;
  }
}

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data,
                       size_t len) {
  buf.insert(buf.end(), data, data + len); // append data the end, next 2 params
                                           // are pointer to start and end.
}

// Remove items from the front...
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
  buf.erase(buf.begin(), buf.begin() + n);
}

// application callback when listening to socket is ready...
static Conn *handle_accept(int fd) {
  struct sockaddr_in client_addr = {};
  socklen_t addrlen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
  if (connfd < 0) {
    return NULL;
  }

  uint32_t ip = client_addr.sin_addr.s_addr;
  fprintf(stderr, "new client from %u.%u.%u.%u:%u\n", ip & 255, (ip >> 8) & 255,
          (ip >> 16) & 255, ip >> 24, ntohs(client_addr.sin_port));

  fd_set_nb(connfd);

  Conn *conn = new Conn();
  conn->fd = connfd;
  conn->want_read = true;
  return conn;
}

const size_t k_max_msg = 32 << 20;
static bool try_one_request(Conn *conn) {
  // try to parse the protocol
  if (conn->incoming.size() < 4) {
    return false;
  }

  uint32_t len;
  memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg) {
    cout << "message too long" << endl;
    conn->want_close = true;
    return false;
  }

  if (4 + len > conn->incoming.size()) {
    return false; // want read
  }

  const uint8_t *request = &conn->incoming[4];
  printf("client says: len:%d data:%.*s\n", len, len < 100 ? len : 100,
         request);

  // generate the response
  buf_append(conn->outgoing, (const uint8_t *)&len, 4);
  buf_append(conn->outgoing, request, len);

  // application logic done, remove the request
  buf_consume(conn->incoming, 4 + len);

  return true;
}

static void handle_write(Conn *conn) {
  assert(conn->outgoing.size() > 0);
  ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());
  if (rv < 0 && errno == EAGAIN) {
    return; // not ready
  }

  if (rv < 0) {
    conn->want_close = true;
    return;
  }

  buf_consume(conn->outgoing, (size_t)rv);

  if (conn->outgoing.size() == 0) {
    conn->want_read = true;
    conn->want_write = false;
  }
}

static void handle_read(Conn *conn) {
  uint8_t buf[64 * 1024];
  ssize_t rv = read(conn->fd, buf, sizeof(buf));
  if (rv < 0 && errno == EAGAIN) {
    return;
  }

  // handle errno
  if (rv < 0) {
    conn->want_close = true;
    return;
  }

  // handle EOF
  if (rv == 0) {
    if (conn->incoming.size() == 0) {
      cout << "client closed" << endl;
    } else {
      cout << "unexpected EOF" << endl;
    }

    conn->want_close = true;
    return;
  }

  buf_append(conn->incoming, buf, (size_t)rv);

  while (try_one_request(conn)) {
  }

  if (conn->outgoing.size() > 0) {
    conn->want_read = false;
    conn->want_write = true;

    return handle_write(conn);
  }
}

int main() {

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    cout << "could not create the socket" << endl;
  }

  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {};
  addr.sin_port = ntohs(1234);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ntohl(0);

  int rv = bind(fd, (const sockaddr *)&addr,
                sizeof(addr)); // Return zero value when success.
  if (rv) {
    cout << "Could not bind to the port";
    return -1;
  }

  // set listen fd to non blocking mode...
  fd_set_nb(fd);

  rv = listen(fd, SOMAXCONN); // return zero when success;
  if (rv) {
    cout << "could to start listening to the port" << endl;
  }

  std::vector<Conn *> fd2conn;
  std::vector<struct pollfd> poll_args;

  while (true) {
    poll_args.clear();
    struct pollfd pfd = {fd, POLLIN, 0};
    poll_args.push_back(pfd);

    for (Conn *conn : fd2conn) {
      if (!conn)
        continue;

      struct pollfd pfd = {conn->fd, POLLERR, 0}; // poll for errors
      if (conn->want_read) {
        pfd.events |= POLLIN;
      }

      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }

      poll_args.push_back(pfd);
    }

    int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
    if (rv < 0 && errno == EINTR) {
      continue;
    }

    if (rv < 0) {
      cout << "poll failed \n";
    }

    // handle listening to the ports...
    if (poll_args[0].revents) {
      if (Conn *conn = handle_accept(fd)) {
        if (fd2conn.size() <= (size_t)conn->fd) {
          fd2conn.resize(conn->fd + 1);
        }

        assert(!fd2conn[conn->fd]);
        fd2conn[conn->fd] = conn;
      }
    }

    // handle connection sockets...
    for (size_t i = 1; i < poll_args.size(); ++i) {
      uint32_t ready = poll_args[i].revents;
      if (ready == 0) {
        continue;
      }

      Conn *conn = fd2conn[poll_args[i].fd];
      if (ready & POLLIN) {
        assert(conn->want_read);
        handle_read(conn);
      }

      if (ready & POLLOUT) {
        assert(conn->want_write);
        handle_write(conn);
      }

      if ((ready & POLLERR) || conn->want_close) {
        (void)close(conn->fd);
        fd2conn[conn->fd] = NULL;
        delete conn;
      }
    }
  }

  return 0;
}
