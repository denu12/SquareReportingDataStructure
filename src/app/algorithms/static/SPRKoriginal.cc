// SPRK (original): relaxed output contract, materialized IDs only.
// Upstream credit and adaptations: src/third_party/esa2026/NOTICE.txt.
#include "app/algorithms/static/esa2026.hh"
#include "benchmark/esa_campaign.hh"

namespace diskreport::sfc::app::algorithms::sfc {
namespace {
esa_campaign::Result run_sprk_ids(const esa_campaign::Dataset& data, bool verify) {
  if (!data.events.empty()) throw std::runtime_error("SPRK IDs entry is static only");
  auto& api = EsaApi::get();
  if (!api.circle_candidates) throw std::runtime_error("SPRK ID bridge unavailable");
  esa_campaign::Result result;
  result.output_contract = "point_ids_only";
  result.membership_contract = "expanded_f64_ball";
  auto started = std::chrono::steady_clock::now();
  std::vector<uint32_t> points;
  points.reserve(2 * data.points.size());
  for (auto p : data.points) { points.push_back(p.x); points.push_back(p.y); }
  static const uint32_t empty[2] = {};
  void* index = api.create(0, points.empty() ? empty : points.data(), data.points.size());
  struct Owner { void* index; EsaApi& api; ~Owner() { api.destroy(index); } } owner{index, api};
  result.build_seconds = esa_campaign::elapsed(started);
  auto batch_start=std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.queries.size(); ++i) {
    const auto& circle = data.queries[i];
    const size_t* ids = nullptr;
    const uint32_t* unused_points = nullptr;
    if(verify)started = std::chrono::steady_clock::now();
    // Historical floating-point IDs-only path: omit exact integer membership
    // and coordinate output from timing. Verification still checks the oracle.
    const double radius = std::sqrt(double(circle.radius2()) / 18446744073709551616.0
                                   + 64 * std::numeric_limits<double>::epsilon());
    const size_t count = api.circle_candidates(index, circle.x, circle.y, radius,
                                              &ids, &unused_points);
    result.answers += count;
    ++result.queries;
    if(verify)result.query_seconds += esa_campaign::elapsed(started);
    // Check actual IDs against the integer oracle outside query timing.
    if (verify) {
      std::vector<uint64_t> actual;
      actual.reserve(count);
      for (size_t j = 0; j < count; ++j) {
        if (ids[j] >= data.points.size()) {
          result.correct = false; result.failed_query = i; return result;
        }
        const auto p = data.points[ids[j]];
        actual.push_back(esa_campaign::key(p.x, p.y));
      }
      std::sort(actual.begin(), actual.end());
      if (actual != esa_campaign::truth(data, circle)) ++result.integer_mismatch_queries;
      // Independent scan of the declared floating contract, outside query timing.
      std::vector<uint64_t> expected;
      const double cx = double(circle.x) / 4294967296.0;
      const double cy = double(circle.y) / 4294967296.0;
      for (auto p : data.points) {
        const double dx = double(p.x) / 4294967296.0 - cx;
        const double dy = double(p.y) / 4294967296.0 - cy;
        if (std::fma(dy, dy, dx * dx) <= radius * radius)
          expected.push_back(esa_campaign::key(p.x, p.y));
      }
      std::sort(expected.begin(), expected.end());
      if (actual != expected) {
        result.correct = false; result.failed_query = i; return result;
      }
    }
  }
  if(!verify)result.query_seconds=esa_campaign::elapsed(batch_start);
 return result;
}
const bool registered = [] {
  esa_campaign::registry()["esa_sprk_CHEATING_IDS_ONLY"] = run_sprk_ids;
  return true;
}();
}  // namespace
}  // namespace diskreport::sfc::app::algorithms::sfc
