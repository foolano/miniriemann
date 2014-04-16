#ifndef CORE_MOCK_CORE_H
#define CORE_MOCK_CORE_H

#include <core/core.h>
#include <index/mock_index.h>

class mock_core : public core_interface {
public:
  mock_core();
  void start();
  void add_stream(std::shared_ptr<streams_t> stream);
  std::shared_ptr<class index> index();
  void send_to_graphite(const std::string host, const int port,
                        const Event & event);
  void forward(const std::string, const int port, const Event & event);
  std::shared_ptr<mock_index> mock_index_impl();
  std::shared_ptr<class scheduler> sched();

private:
  std::shared_ptr<class scheduler> sched_;
  std::shared_ptr<mock_index> mock_index_;
  std::shared_ptr<class index> index_;
};

#endif
