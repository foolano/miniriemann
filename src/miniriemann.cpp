#include <glog/logging.h>
#include <async/async_loop.h>
#include <thread>
#include <index/real_index.h>
#include <transport/listen_tcp_socket.h>
#include <riemann_tcp_pool.h>
#include <websocket_pool.h>
#include <streams/stream_functions.h>
#include <util.h>
#include <pub_sub/pub_sub.h>
#include <query/driver.h>
#include "pagerduty.h"
#include <incoming_events.h>
#include <riemann_tcp_pool.h>
#include <riemann_udp_pool.h>
#include <scheduler/scheduler.h>
#include <atom/atom.h>


void detach_thread(std::function<void()> fn) {
  std::thread t(fn);
  t.detach();
}

int main(int, char **argv)
{
  atom_initialize();
  {
    ATOM_GC;
    atom_attach_thread();

    google::InitGoogleLogging(argv[0]);

    auto main_loop = create_main_async_loop();

    pub_sub pubsub;
    real_index real_idx(pubsub, [](e_t){}, 60, detach_thread);
    class index idx(real_idx);

    streams streams;
    //streams.add_stream(send_index(idx));

    incoming_events incoming(streams);

    riemann_tcp_pool rieman_tcp{1, [&](const std::vector<unsigned char> e)
                                        {
                                          incoming.add_undecoded_msg(e);
                                        }};

    main_loop.add_tcp_listen_fd(create_tcp_listen_socket(5555),
                                [&](int fd) { rieman_tcp.add_client(fd); });

    riemann_udp_pool rieman_udp{[&](const std::vector<unsigned char> e)
                                    {
                                      incoming.add_undecoded_msg(e);
                                    }};

    websocket_pool ws(1, pubsub);
    main_loop.add_tcp_listen_fd(create_tcp_listen_socket(5556),
                                [&](int fd) {ws.add_client(fd); });


    main_loop.start();
  }

  atom_terminate();

  return 0;
}
