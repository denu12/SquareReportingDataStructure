#include "app/algorithms/dynamic/reporting/new_dynamic.hh"
#include "app/algorithms/dynamic/reporting/adapter.hh"
#include <iostream>
using namespace diskreport::app::algorithms::dynamic::sfc;
using namespace sfc_dynamic_competitors;
template<class T>struct Probe {
 using PointType=Point;static constexpr int offset=0;
 struct Type {template<class A,class B>Type(const A&,const B&){} void handle_insertion(Point){}void handle_deletion(Point){}bool handle_query(Box){return true;}bool valid(){return true;}size_t size(){return 0;}};
};
int main(){
 for(auto dist:{"norm","skew","unif"})for(int interleave:{0,1}){
  diskreport::app::app_io::Instance i;i.set_seed(1);i.set_construct_dist(dist);i.set_query_dist("unif");i.set_insertion_count(1024);i.set_deletion_count(512);i.set_query_count(2048);i.set_mix_deletions(true);(*i.mutable_double_params())["fixed_w"]=.25;(*i.mutable_int64_params())["dynamic_interleave"]=interleave;
  DynSFCInMemory<Probe,uint32_t>g;auto result=g.Generate({},i);if(!result.ok())return 1;auto&tasks=(*result)->tasks;
  std::unordered_map<uint64_t,size_t>live;size_t ins=0,del=0,queries=0,reports=0;bool saw_query_before_last_insert=false;
  using Mode=InMemoryQueryInstance<Point>::Mode;
  for(auto [q,m]:tasks){if(m==Mode::Insertion){++live[key(q.first)];++ins;}else if(m==Mode::Delete){auto it=live.find(key(q.first));if(it==live.end()||!it->second)throw std::runtime_error("Invalid deletion order");if(!--it->second)live.erase(it);++del;}else{++queries;if(ins<1024)saw_query_before_last_insert=true;for(auto [k,n]:live)if(inside(Point(k>>32,uint32_t(k)),q))reports+=n;}}
  if(ins!=1024||del!=512||queries!=2048||saw_query_before_last_insert!=bool(interleave))return 2;
  std::cout<<dist<<" "<<interleave<<" "<<reports<<"\n";
 }
}
