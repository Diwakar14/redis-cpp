#include <iostream>
#include <string>
#include <sys/socket.h>

using namespace std;

int main() {
  cout << "Redis Server\n";

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    cout << "Could not create socket stream";
  } else {
    cout << "Create socket stream" << fd << "::" << endl;
  }

  return 0;
}
