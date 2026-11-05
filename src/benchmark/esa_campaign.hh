#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "benchmark/circle.hh"

namespace esa_campaign {
struct XY { uint32_t x, y; };
struct Event { uint8_t kind; Circle value; }; // 0 insert, 1 delete one occurrence, 2 query
struct Dataset { std::vector<XY> points; std::vector<Circle> queries; std::vector<Event> events; };
struct Result { double build_seconds=0, query_seconds=0, update_seconds=0; uint64_t answers=0, queries=0, insertions=0, deletions=0; std::string output_contract="integer_points", membership_contract="closed_integer_ball"; uint64_t integer_mismatch_queries=0; bool correct=true; int64_t failed_query=-1; };
using Runner = std::function<Result(const Dataset&, bool)>;
inline std::map<std::string, Runner>& registry() { static std::map<std::string, Runner> r; return r; }
inline bool selected(const std::string& name) {
  static const std::vector<std::string> names={"Morton","MortonSIMD","boost_lin","boost_quad","boost_star","cgal_rt","cgal_kd","thst_quad","thst_rtree","pargeo","pam","pkd","naive","chan_sss","ann_fr","stann_fr","esa_sprk_CHEATING_IDS_ONLY","esa_kiddo","esa_nabo","esa_neighbourhood","esa_vptree","esa_orthtree","esa_grid","esa_sklearn_kd","esa_sklearn_ball","esa_snn","esa_nanoflann","boost_lin_dyn","boost_quad_dyn","boost_star_dyn","chan_sss_dyn_ADAPTED_DYNAMIC","kiddo_mutable_dyn_UPSTREAM","nanoflann_dyn_UPSTREAM","pkd_dyn_UPSTREAM","thst_rtree_dyn_UPSTREAM","thst_quad_dyn_UPSTREAM"};
  return std::find(names.begin(),names.end(),name)!=names.end();
}
inline bool ours(const std::string& n) { return n == "Morton" || n == "MortonSIMD"; }
inline Dataset load(const std::string& path) {
  std::ifstream f(path,std::ios::binary);
  char magic[8]; uint64_t n=0,q=0;
  f.read(magic,8); f.read(reinterpret_cast<char*>(&n),8); f.read(reinterpret_cast<char*>(&q),8);
  if (!f || (std::string(magic,8)!=std::string("ESA2D01\0",8) && std::string(magic,8)!=std::string("DRRDYN1\0",8)) || n>1000000000 || q>1000000000) throw std::runtime_error("Invalid ESA2D data header");
  bool dynamic=std::string(magic,8)==std::string("DRRDYN1\0",8);
  Dataset d; d.points.resize(n); if(dynamic)d.events.resize(q);else d.queries.resize(q);
  for(auto& p:d.points) {f.read(reinterpret_cast<char*>(&p.x),4); f.read(reinterpret_cast<char*>(&p.y),4);}
  for(auto& c:d.queries) {f.read(reinterpret_cast<char*>(&c.x),4);f.read(reinterpret_cast<char*>(&c.y),4);f.read(reinterpret_cast<char*>(&c.radius2_lo),8);f.read(reinterpret_cast<char*>(&c.radius2_hi),8);}
  for(auto& e:d.events){ f.read(reinterpret_cast<char*>(&e.kind),1);f.read(reinterpret_cast<char*>(&e.value.x),4);f.read(reinterpret_cast<char*>(&e.value.y),4);f.read(reinterpret_cast<char*>(&e.value.radius2_lo),8);f.read(reinterpret_cast<char*>(&e.value.radius2_hi),8);if(e.kind>2)throw std::runtime_error("Invalid dynamic operation");}
  if(!f || f.peek()!=EOF) throw std::runtime_error("Truncated or trailing ESA2D data");
  return d;
}
inline uint64_t key(uint32_t x,uint32_t y){return (uint64_t(x)<<32)|y;}
template<class P> XY xy(const P& p) {
  if constexpr(requires{p.x();p.y();})return {uint32_t(p.x()),uint32_t(p.y())};
  else if constexpr(requires{p.y;})return {uint32_t(p.x),uint32_t(p.y)};
  else if constexpr(requires{p.pnt[0];})return {uint32_t(p.pnt[0]),uint32_t(p.pnt[1])};
  else return {uint32_t(p.x[0]),uint32_t(p.x[1])};
}
inline std::pair<XY,XY> bounds(const Circle& c) {
  uint64_t r=uint64_t(std::ceil(std::sqrt(static_cast<long double>(c.radius2()))));
  // Nondegenerate traversal envelope also handles zero-radius queries without
  // changing the exact circle predicate. The original SFC traversal uses clz.
  r=std::max<uint64_t>(r,1);
  return {{uint32_t(c.x>r?c.x-r:0),uint32_t(c.y>r?c.y-r:0)},
          {uint32_t(std::min<uint64_t>(UINT32_MAX,uint64_t(c.x)+r)),uint32_t(std::min<uint64_t>(UINT32_MAX,uint64_t(c.y)+r))}};
}
inline double elapsed(std::chrono::steady_clock::time_point t) {return std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();}
// Independent oracle: signed wide-integer arithmetic, no call to contains().
inline std::vector<uint64_t> truth(const Dataset& d,const Circle& c) {
  std::vector<uint64_t> out;
  for(auto p:d.points){__int128 dx=__int128(p.x)-c.x,dy=__int128(p.y)-c.y;
    if(static_cast<unsigned __int128>(dx*dx+dy*dy)<=c.radius2())out.push_back(key(p.x,p.y));}
  std::sort(out.begin(),out.end());return out;
}
// Output iterator: only accepted integer points enter the materialized result.
template<class P> struct CircleOutput {
 using iterator_category=std::output_iterator_tag;using difference_type=std::ptrdiff_t;using value_type=void;using pointer=void;using reference=void;
 std::vector<P>* output;const Circle* circle;
 CircleOutput& operator*(){return *this;}CircleOutput& operator++(){return *this;}CircleOutput operator++(int){return *this;}
 CircleOutput& operator=(const P& p){auto a=xy(p);if(circle->contains(a.x,a.y))output->push_back(p);return *this;}
};

template<class P,class Query> Result queries(const Dataset& d,bool verify,bool integrated,Query query,Result r,std::vector<P>* reusable=nullptr,bool native_radius=false) {
  std::vector<P> local_output;auto& output=reusable?*reusable:local_output;
  auto batch_start=std::chrono::steady_clock::now();
  for(size_t i=0;i<d.queries.size();++i){const auto& c=d.queries[i];
    auto start=verify?std::chrono::steady_clock::now():batch_start;
    auto [lo,hi]=native_radius?std::pair{XY{c.x,c.y},XY{c.x,c.y}}:bounds(c);
    active_circle=&c;
    output.clear();query(lo,hi,output);
    active_circle=nullptr;
    // Rectangle-only competitors use the standard timed circle adapter.
    if(!integrated)std::erase_if(output,[&](const auto& p){auto a=xy(p);return !c.contains(a.x,a.y);});
    r.answers+=output.size();++r.queries;if(verify)r.query_seconds+=elapsed(start);
    if(verify){std::vector<uint64_t> got;got.reserve(output.size());for(const auto& p:output){auto a=xy(p);got.push_back(key(a.x,a.y));}std::sort(got.begin(),got.end());
      if(got!=truth(d,c)){r.correct=false;r.failed_query=i;return r;}}
  }
  if(!verify)r.query_seconds+=elapsed(batch_start);
  return r;
}
template<class P,class Index,class Convert> Result dynamic_queries(const Dataset& d,bool verify,bool integrated,Index& index,Convert point,Result r) {
  if(d.events.empty())return queries<P>(d,verify,integrated,[&](auto lo,auto hi,auto& out){index.esa_query(point(lo),point(hi),out);},r);
  Dataset live;if(verify)live.points=d.points;
  std::vector<P> output;
  size_t query_number=0;
  for(const auto& e:d.events){
    XY p{e.value.x,e.value.y};
    if(e.kind==2){
      live.queries.assign(1,e.value);
      r=queries<P>(live,verify,integrated,[&](auto lo,auto hi,auto& out){index.esa_query(point(lo),point(hi),out);},r,&output);
      if(!r.correct){r.failed_query=query_number;return r;}
      ++query_number;
    }else{
      auto start=std::chrono::steady_clock::now();
      if(e.kind==0){index.handle_insertion(point(p));++r.insertions;}
      else{index.handle_deletion(point(p));++r.deletions;}
      r.update_seconds+=elapsed(start);
      if(verify){
        if(e.kind==0)live.points.push_back(p);
        else{auto it=std::find_if(live.points.begin(),live.points.end(),[&](auto a){return a.x==p.x&&a.y==p.y;});
          if(it==live.points.end())throw std::runtime_error("Invalid deletion in workload");live.points.erase(it);}
      }
    }
  }
  return r;
}

}
