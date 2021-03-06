#ifndef CAVALIERI_EXTERNAL_GRAPHITE_H
#define CAVALIERI_EXTERNAL_GRAPHITE_H

#include <common/event.h>
#include <mutex>
#include <config/config.h>
#include <external/graphite_pool.h>

class graphite {
public:
  graphite(const config conf);
  void push_event(const std::string host, const int port, const Event & event);

private:
  const config config_;
  std::unordered_map<std::string,
                     std::shared_ptr<graphite_pool>> pool_map_;
  std::mutex mutex_;

};

#endif
