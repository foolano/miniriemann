#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <glog/logging.h>

namespace {
const uint32_t k_listen_backlog = 100;
}

int create_tcp_listen_socket(int port) {

  struct sockaddr_in addr;

  int socket_fd  = socket(PF_INET, SOCK_STREAM, 0);

  int reuse_addr = 1;

  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                 sizeof(reuse_addr)) != 0)
  {
    LOG(ERROR) << "failed to set SO_REUSEADDR in socket";
  }

  /*
  int recv_buf = 21299200;

  if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf,
                 sizeof(recv_buf)) != 0)
  {
    LOG(ERROR) << "failed to set SO_REUSEADDR in socket";
  }

  if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &recv_buf,
                 sizeof(recv_buf)) != 0)
  {
    LOG(ERROR) << "failed to set SO_REUSEADDR in socket";
  }
  */





  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    LOG(FATAL) << "bind error: " << strerror(errno);
    exit(1);
  }

  fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) | O_NONBLOCK);
  listen(socket_fd, k_listen_backlog);

  return socket_fd;
}
