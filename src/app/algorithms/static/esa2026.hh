#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <dlfcn.h>
#include <memory>
#include <stdexcept>
#include <vector>
#include "benchmark/common.hh"
#include "third_party/esa2026/nanoflann.hpp"
namespace diskreport::sfc::app::algorithms::sfc {
struct EsaApi {
 using Create=void*(*)(uint32_t,const uint32_t*,size_t);
 using Destroy=void(*)(void*);
 using Search=size_t(*)(void*,uint32_t,uint32_t,uint32_t,uint32_t,const uint32_t**);
 using CircleSearch=size_t(*)(void*,uint32_t,uint32_t,double,const uint32_t**);
 using CircleCandidates=size_t(*)(void*,uint32_t,uint32_t,double,const size_t**,const uint32_t**);
 CircleCandidates circle_candidates;
 using PrepareRuntime=void(*)(uint32_t);
 PrepareRuntime prepare_runtime;
 using CreateHint=void*(*)(uint32_t,const uint32_t*,size_t,double);
 CreateHint create_hint;
 Create create; Destroy destroy; Search search; CircleSearch circle_search;
 EsaApi(){const char* path=std::getenv("SFC_ESA_LIBRARY");void* lib=dlopen(path?path:"libsfc_esa2026.so",RTLD_NOW|RTLD_GLOBAL);
 if(!lib)throw std::runtime_error(dlerror());
 create_hint=reinterpret_cast<CreateHint>(dlsym(lib,"esa_create_with_hint"));
 create=reinterpret_cast<Create>(dlsym(lib,"esa_create"));destroy=reinterpret_cast<Destroy>(dlsym(lib,"esa_destroy"));search=reinterpret_cast<Search>(dlsym(lib,"esa_query"));
 circle_search=reinterpret_cast<CircleSearch>(dlsym(lib,"esa_circle_query"));
 circle_candidates=reinterpret_cast<CircleCandidates>(dlsym(lib,"esa_circle_candidates"));
 prepare_runtime=reinterpret_cast<PrepareRuntime>(dlsym(lib,"esa_prepare_runtime"));
 if(!create||!destroy||!search||!prepare_runtime)throw std::runtime_error("Invalid ESA bridge ABI");}
 static EsaApi& get(){static EsaApi api;return api;}
};
template<unsigned Kind> class Esa2026 {
 public:
 static void prepare_runtime(){auto& api=EsaApi::get();api.prepare_runtime(Kind);}
 static constexpr bool uses_radius_hint=Kind==6;
 static constexpr bool reports_exact_circle=true;
 static constexpr bool uses_native_radius=true;
 using IDXT=uint32_t; using PointType=Point<IDXT>;using QueryType=Query<PointType>;
 explicit Esa2026(const std::vector<PointType>& points,double radius_hint=4294967.296){std::vector<uint32_t> xy;xy.reserve(2*points.size());for(auto p:points){xy.push_back(p.x);xy.push_back(p.y);}static const uint32_t empty[2]={};if(!EsaApi::get().create_hint)throw std::runtime_error("Radius-hint bridge unavailable");index_=EsaApi::get().create_hint(Kind,xy.empty()?empty:xy.data(),points.size(),radius_hint/4294967296.0);}
 ~Esa2026(){EsaApi::get().destroy(index_);}
 Esa2026(const Esa2026&)=delete;Esa2026& operator=(const Esa2026&)=delete;
 template<class Output>void query(const QueryType&q,Output out)const{const uint32_t* xy;
 if(auto c=::esa_campaign::active_circle){
  if(!EsaApi::get().circle_candidates)throw std::runtime_error("ESA campaign requires enumerated circle bridge ABI");
  double squared=double(c->radius2())/18446744073709551616.0*(1+128*std::numeric_limits<double>::epsilon());
  // SNN subtracts absolute dot products: cancellation needs an absolute allowance.
  if constexpr(Kind==9)squared+=128*std::numeric_limits<double>::epsilon();
  double r=std::nextafter(std::sqrt(std::nextafter(squared,INFINITY)),INFINITY);
  if constexpr(Kind==3){
   auto n=EsaApi::get().circle_search(index_,c->x,c->y,r,&xy);
   for(size_t i=0;i<n;++i)if(c->contains(xy[2*i],xy[2*i+1]))*out++=PointType{xy[2*i],xy[2*i+1]};return;
  }
  const size_t* ids;
  auto n=EsaApi::get().circle_candidates(index_,c->x,c->y,r,&ids,&xy);
  for(size_t i=0;i<n;++i){auto id=ids[i];auto x=xy[2*id],y=xy[2*id+1];if(c->contains(x,y))*out++=PointType{x,y};}return;
 }
auto n=EsaApi::get().search(index_,q.p.x,q.p.y,q.q.x,q.q.y,&xy);for(size_t i=0;i<n;++i)*out++=PointType{xy[2*i],xy[2*i+1]};}
 private:void* index_;
};
class EsaNanoflann {
 public:
 static constexpr bool reports_exact_circle=true;
 static constexpr bool uses_native_radius=true;
 using IDXT=uint32_t; using PointType=Point<IDXT>;using QueryType=Query<PointType>;
 struct Cloud {std::vector<PointType> p;size_t kdtree_get_point_count()const{return p.size();}double kdtree_get_pt(size_t i,size_t d)const{return double(d?p[i].y:p[i].x)/4294967296.0;}template<class B>bool kdtree_get_bbox(B&)const{return false;}};
 using Tree=nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double,Cloud>,Cloud,2>;
 explicit EsaNanoflann(const std::vector<PointType>&p):cloud_{p},tree_(2,cloud_,nanoflann::KDTreeSingleIndexAdaptorParams(10)){}
 template<class Output>void query(const QueryType&q,Output out)const{
 if(auto c=::esa_campaign::active_circle){
 double center[2]={double(c->x)/4294967296.0,double(c->y)/4294967296.0};hits_.clear();
 tree_.radiusSearch(center,std::nextafter(double(c->radius2())/18446744073709551616.0*(1+128*std::numeric_limits<double>::epsilon()),INFINITY),hits_,nanoflann::SearchParameters(0,false));
 for(auto h:hits_){auto p=cloud_.p[h.first];if(c->contains(p.x,p.y))*out++=p;}return;
 }
 if(q.p.x>q.q.x||q.p.y>q.q.y)return;
 double c[2]={(double(q.p.x)+q.q.x)/8589934592.0,(double(q.p.y)+q.q.y)/8589934592.0};
 double dx=(double(q.q.x)-q.p.x)/8589934592.0,dy=(double(q.q.y)-q.p.y)/8589934592.0;
 hits_.clear();tree_.radiusSearch(c,dx*dx+dy*dy+64*std::numeric_limits<double>::epsilon(),hits_,nanoflann::SearchParameters(0,false));
 for(auto h:hits_){auto p=cloud_.p[h.first];if(p.x>=q.p.x&&p.x<=q.q.x&&p.y>=q.p.y&&p.y<=q.q.y)*out++=p;}}
 private:Cloud cloud_;Tree tree_;mutable std::vector<nanoflann::ResultItem<unsigned int,double>> hits_;
};
}
