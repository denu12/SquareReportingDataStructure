#include <sys/types.h>

#include <utility>
#include <limits>
#undef LOG
#define PARLAY_SEQUENTIAL
#include <pam/pam.h>

#include "pam_ex/range_utils.h"
//
#include "pam_ex/range_tree.h"
//

//
#include "app/algorithms/static/adapter.hh"

namespace diskreport::sfc::app::algorithms::sfc {
template <typename Input_t>
struct PAM {
  using IDXT = Input_t;
  using PointType = ::Point<IDXT, uint32_t>;
  using IDX = ::RangeQuery<IDXT, uint32_t>;
  struct Query {
    PointType p, q;
    Query(PointType p, PointType q) : p(p), q(q){};
  };
  using QueryType = Query;
  typename IDX::outer_map index;
  PAM(std::vector<PointType>& points) {
    IDX::reserve(points.size());
    auto entries=parlay::tabulate(points.size(),[&](size_t i)->typename IDX::main_entry {
      return {{points[i].x,points[i].y},1};
    });
    // Native construction combines equal keys; no extra sorting pass in the adapter.
    index=typename IDX::outer_map(entries,[](uint32_t a,uint32_t b){return a+b;});
  }

  template<class Output> void query(const QueryType& q, Output out) {
    // One range_sum traversal. The local callback emits x,y (inner keys are y,x).
    // Multiplicity is stored as an exact integer map value at construction.
    struct Reporter {
      IDXT low,high;Output& out;
      void emit(IDXT x,IDXT y,uint32_t count) {
        for(uint32_t i=0;i<count;++i)*out++=PointType(x,y,1);
      }
      void add_entry(typename IDX::main_entry entry) {
        auto [x,y]=entry.first;if(y>=low && y<=high)emit(x,y,entry.second);
      }
      void add_aug_val(typename IDX::inner_map map) {
        auto range=IDX::inner_map::range(map,{low,0},{high,std::numeric_limits<IDXT>::max()});
        IDX::inner_map::foreach_index(range,[&](const auto& entry,size_t){emit(entry.first.second,entry.first.first,entry.second);},0,std::numeric_limits<size_t>::max());
      }
    } report{q.p.y,q.q.y,out};
    index.range_sum({q.p.x,0},{q.q.x,std::numeric_limits<IDXT>::max()},report);
  }

};
template <typename Tp>
struct PAMImpl : public Algo<PAM<Tp>> {
  using PK = PAM<Tp>;
  PAMImpl()
      : Algo<PAM<Tp>>([](typename PK::IDXT a, typename PK::IDXT b) {
          return typename PK::PointType(a, b, 1.0);
        }){};
};
using PAMImpl32 = PAMImpl<u_int32_t>;
using PAMImpl16 = PAMImpl<u_int16_t>;
REGISTER_IMPL_NAMED(PAMImpl16, "pam16");

REGISTER_IMPL_NAMED(PAMImpl32, "pam");
}  // namespace diskreport::sfc::app::algorithms::sfc