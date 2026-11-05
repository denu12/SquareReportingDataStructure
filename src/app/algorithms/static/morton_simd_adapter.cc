#include "morton/2d/mortonSIMD.cc"
#include "app/algorithms/static/adapter.hh"
namespace diskreport::morton_MortonSIMD {
class BenchmarkMortonSIMD : public MortonSIMD {
 public:
  using MortonSIMD::MortonSIMD;
  void query(const Box& box, auto output) const {
    if (const auto* c = ::esa_campaign::active_circle) {
      const Circle circle{{c->x, c->y}, (Wide(c->radius2_hi) << 64) | c->radius2_lo};
      scan(box, &circle, output);
    } else MortonSIMD::query(box, output);
  }
};
using Adapter = ::diskreport::sfc::app::algorithms::sfc::Algo<BenchmarkMortonSIMD>;
REGISTER_IMPL_NAMED(Adapter, "MortonSIMD");
}
