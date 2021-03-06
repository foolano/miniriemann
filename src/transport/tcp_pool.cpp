#include <glog/logging.h>
#include <functional>
#include <transport/tcp_pool.h>
#include <transport/tcp_connection.h>
#include <util/util.h>

using namespace std::placeholders;

tcp_pool::tcp_pool(
    size_t thread_num,
    hook_fn_t run_fn,
    tcp_create_conn_fn_t tcp_create_conn_fn,
    tcp_ready_fn_t tcp_ready_fn)
:
  async_thread_pool_(thread_num),
  mutexes_(thread_num),
  new_fds_(thread_num),
  conn_maps_(thread_num),
  tcp_create_conn_fn_(tcp_create_conn_fn),
  tcp_ready_fn_(tcp_ready_fn),
  async_fn_()
{
  async_thread_pool_.set_async_hook(std::bind(&tcp_pool::async_hook, this, _1));
  async_thread_pool_.set_run_hook(run_fn);
}

tcp_pool::tcp_pool(
    size_t thread_num,
    hook_fn_t run_fn,
    tcp_create_conn_fn_t tcp_create_conn_fn,
    tcp_ready_fn_t tcp_ready_fn,
    async_fn_t async_fn)
:
  async_thread_pool_(thread_num),
  mutexes_(thread_num),
  new_fds_(thread_num),
  conn_maps_(thread_num),
  tcp_create_conn_fn_(tcp_create_conn_fn),
  tcp_ready_fn_(tcp_ready_fn),
  async_fn_(async_fn)
{
  async_thread_pool_.set_async_hook(std::bind(&tcp_pool::async_hook, this, _1));
  async_thread_pool_.set_run_hook(run_fn);
}

void tcp_pool::add_client(const int fd) {
  VLOG(3) << "add_client() sfd: " << fd;

  size_t tid = async_thread_pool_.next_thread();
  VLOG(3) << "next thread: " << tid;
  mutexes_[tid].lock();
  new_fds_[tid].push(fd);
  mutexes_[tid].unlock();

  async_thread_pool_.signal_thread(tid);
}

void tcp_pool::add_client(const size_t loop_id, const int fd) {
  VLOG(3) << "add_client() sfd: " << fd << " to loop_id: " << loop_id;

  mutexes_[loop_id].lock();
  new_fds_[loop_id].push(fd);
  mutexes_[loop_id].unlock();

  async_thread_pool_.signal_thread(loop_id);
}

void tcp_pool::add_client_sync(const size_t loop_id, const int fd) {

  VLOG(3) << "add_client_sync() sfd: " << fd << " to loop_id: " << loop_id;

  loop(loop_id).add_fd(fd, async_fd::none,
                       std::bind(&tcp_pool::socket_callback, this, _1));

   conn_maps_[loop_id].insert({fd, tcp_connection(fd)});

}

void tcp_pool::remove_client_sync(const size_t loop_id, int fd) {

  VLOG(3) << "remove_client_sync() sfd: " << fd << " to loop_id: " << loop_id;

  VLOG(3) << "removing tcp_client date for fd";
  auto it  = conn_maps_[loop_id].find(fd);
  CHECK(it != conn_maps_[loop_id].end()) << "fd not found";

  conn_maps_[loop_id].erase(it);

  VLOG(3) << "removing fd from loop";
  async_thread_pool_.loop(loop_id).remove_fd(fd);

}


void tcp_pool::signal_threads() {
  VLOG(3) << "signaling all threads";

  for (size_t i = 0; i < mutexes_.size(); i++) {
    async_thread_pool_.signal_thread(i);
  }

}

void tcp_pool::signal_thread(const size_t loop_id) {
  VLOG(3) << "signaling thread: " << loop_id;

  async_thread_pool_.signal_thread(loop_id);

}

void tcp_pool::async_hook(async_loop & loop) {

  size_t tid = loop.id();
  VLOG(3) << "async_hook() tid: " << tid;

  if (!new_fds_.empty()) {
    add_fds(loop);
  }

  if (async_fn_) {
    async_fn_(loop);
  }

}

void tcp_pool::add_fds(async_loop & loop) {

  size_t tid = loop.id();

  mutexes_[tid].lock();
  std::queue<int> new_fds(std::move(new_fds_[tid]));
  mutexes_[tid].unlock();

  while (!new_fds.empty()) {
    const int fd = new_fds.front();
    new_fds.pop();
    auto socket_cb = std::bind(&tcp_pool::socket_callback, this, _1);
    loop.add_fd(fd, async_fd::read, socket_cb);
    auto insert = conn_maps_[tid].insert({fd, tcp_connection(fd)});

    if (tcp_create_conn_fn_) {
     tcp_create_conn_fn_(fd, loop, insert.first->second);
    }

    VLOG(3) << "async_hook() tid: " << tid << " adding fd: " << fd;
  }

}


void tcp_pool::socket_callback(async_fd & async) {
  auto tid = async.loop().id();
  VLOG(3) << "socket_callback() tid: " << tid;

  if (async.error()) {
    VLOG(3) << "got invalid event: " << strerror(errno);
  }

  auto it  = conn_maps_[tid].find(async.fd());
  CHECK(it != conn_maps_[tid].end()) << "fd not found";
  auto & conn = it->second;

  tcp_ready_fn_(async, conn);

  if (conn.close_connection) {
    VLOG(3) << "socket_callback() close_connection fd: " << async.fd();
    async.stop();
    conn_maps_[tid].erase(it);
    return;
  }



  VLOG(3) << "--socket_callback() tid: " << tid;
}

void tcp_pool::start_threads() {
  async_thread_pool_.start_threads();
}

void tcp_pool::stop_threads() {
  async_thread_pool_.stop_threads();
}

async_loop & tcp_pool::loop(const size_t id) {
  return async_thread_pool_.loop(id);
}

tcp_pool::~tcp_pool() {
  async_thread_pool_.stop_threads();
}
