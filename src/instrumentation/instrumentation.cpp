#include <glog/logging.h>
#include <algorithm>
#include <instrumentation/instrumentation.h>

namespace {

const int k_max_rates = 1000;
const int k_max_latencies = 1000;
const std::string k_instrumentation_tag = "cavalieri::internal";
const float k_default_ttl = 60;

};

instrumentation::instrumentation(const config conf)
  :
    rates_id_(0),
    latencies_id_(0),
    rates_(k_max_rates),
    latencies_(k_max_latencies)

{
}

int instrumentation::add_rate(const std::string service,
                              const std::string description)
{

  CHECK(rates_id_.load() < k_max_rates) << "max number of rates reached";

  int id = rates_id_.fetch_add(1);

  rates_[id].service = service;
  rates_[id].description = description;
  rates_[id].rate.reset();

  return id;
}

void instrumentation::update_rate(const unsigned int id,
                                  const unsigned int ticks)
{
  rates_[id].rate.add(ticks);
}

int instrumentation::add_latency(const std::string service,
                                 const std::string description,
                                 std::vector<double> percentiles)
{

  CHECK(rates_id_.load() < k_max_latencies)
        << "max number of percentiles reached";

  int id = latencies_id_.fetch_add(1);

  latencies_[id].service = service;
  latencies_[id].description = description;
  latencies_[id].percentiles = percentiles;

  return id;
}

void instrumentation::update_latency(const unsigned int id,
                                     const double value)
{
  latencies_[id].reservoir.add_sample(value);
}

std::vector<Event> instrumentation::snapshot() {

  VLOG(3) << "snapshot()!";

  std::vector<Event> events;

  Event event;

  *(event.add_tags()) = k_instrumentation_tag;

  event.set_host("localhost");
  event.set_state("ok");
  event.set_ttl(k_default_ttl);

  for (int i = 0; i < rates_id_.load(); i++) {

    event.set_service(rates_[i].service);
    event.set_description(rates_[i].description);
    event.set_metric_d(rates_[i].rate.snapshot());

    events.push_back(event);
  }

 for (int i = 0; i < latencies_id_.load(); i++) {

   auto samples = latencies_[i].reservoir.snapshot();


   if (samples.empty()) {
     VLOG(3) << "samples empty";
     continue;
   }

   std::stable_sort(begin(samples), end(samples));

   auto n = samples.size();
   for (const auto & p : latencies_[i].percentiles) {

      auto index = p == 1 ? n - 1 : static_cast<int>(n * p);

      event.set_service(latencies_[i].service + " " + std::to_string(p));
      event.set_description(latencies_[i].description);
      event.set_metric_d(samples[index]);

      events.push_back(event);

   }


 }

  return events;;
}