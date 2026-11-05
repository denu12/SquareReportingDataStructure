#include "app/algorithms/dyn_algorithm_impl.h"
namespace diskreport::app::algorithms {
diskreport::app::app_io::TimePerTickStat processTimeStat(const std::vector<int64_t>& times) {
  diskreport::app::app_io::TimePerTickStat result;
  double ymin = std::numeric_limits<double>::max();
  double ymax = std::numeric_limits<double>::min();
  double sum = 0;
  for (auto t : times) {
    ymin = std::min((double)t, ymin);
    ymax = std::max((double)t, ymax);
    sum += t;
  }
  if (times.size() == 0) {
    return result;
  }
  double mean = sum / ((double)times.size());
  double varianceSum = 0;
  for (auto t : times) {
    varianceSum += (t - mean) * (t - mean);
  }
  double stddev = std::sqrt(varianceSum / ((double)times.size()));
  result.set_min(ymin);
  result.set_max(ymax);
  result.set_mean(mean);
  result.set_stddev(stddev);
  result.set_count(times.size());

  return result;
}
}  // namespace diskreport::app::algorithms