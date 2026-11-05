#include "app/algorithms/dynamic/reporting/new_dynamic.hh"
#include <iostream>
using namespace sfc_dynamic_competitors;
template<class B>void check(const char*name){
 Multiset<B>index;std::vector<Point>live;std::mt19937_64 rng(98121);size_t queries=0;
 auto query=[&](Box q){std::vector<Point>got;index.query(q,got);std::vector<uint64_t>a,b;for(auto p:got)a.push_back(key(p));for(auto p:live)if(inside(p,q))b.push_back(key(p));std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());if(a!=b){std::cerr<<name<<" mismatch live="<<live.size()<<" got="<<a.size()<<" expected="<<b.size()<<"\n";std::abort();}++queries;};
 auto add=[&](Point p){index.insert(p);live.push_back(p);};
 auto del=[&](size_t i){index.erase(live[i]);live.erase(live.begin()+i);};
 query({{0,0},{UINT32_MAX,UINT32_MAX}});
 for(Point p:std::vector<Point>{{0,0},{UINT32_MAX,UINT32_MAX},{0,UINT32_MAX},{UINT32_MAX,0},{1,1},{1,1},{1,1}})add(p);
 query({{0,0},{UINT32_MAX,UINT32_MAX}});query({{1,1},{1,1}});del(4);query({{1,1},{1,1}});
 while(!live.empty()){del(live.size()-1);query({{0,0},{UINT32_MAX,UINT32_MAX}});}
 for(unsigned t=0;t<7000;++t){
  if(live.empty()||rng()%100<55){Point p;if(!live.empty()&&rng()%5==0)p=live[rng()%live.size()];else if(t%3==0)p={uint32_t(rng()%64),uint32_t(rng()%64)};else p={uint32_t(rng()),uint32_t(rng())};add(p);}
  else del(rng()%live.size());
  if(t%7==0){uint32_t a=rng(),b=rng(),c=rng(),d=rng();query({{std::min(a,b),std::min(c,d)},{std::max(a,b),std::max(c,d)}});query({{0,0},{63,63}});if(!live.empty()){auto p=live[rng()%live.size()];query({p,p});}}
 }
 query({{10,10},{9,9}});while(!live.empty())del(live.size()-1);query({{0,0},{UINT32_MAX,UINT32_MAX}});
 if constexpr(!std::is_same_v<B,Kiddo>){for(unsigned n=0;n<96;++n)add({100,n});query({{100,0},{100,UINT32_MAX}});while(!live.empty())del(live.size()-1);}
 add({42,42});query({{42,42},{42,42}});del(0);
 std::cout<<name<<": "<<queries<<" exact multiset checks passed\n";
}
int main(int argc,char**argv){
 if(argc>1&&std::string(argv[1])=="--kiddo-collinear"){
  // Separate reproducer for Kiddo 5.0.3's documented bucket/axis restriction.
  Multiset<Kiddo>index;for(unsigned n=0;n<96;++n)index.insert({100,n});return 0;
 }
 check<ChanTreap>("chan_sss_dyn_ADAPTED_DYNAMIC");check<Kiddo>("kiddo_mutable_dyn_UPSTREAM");check<Nanoflann>("nanoflann_dyn_UPSTREAM");check<Pkd>("pkd_dyn_UPSTREAM");check<ThstRtree>("thst_rtree_dyn_UPSTREAM");check<ThstQuad>("thst_quad_dyn_UPSTREAM");
}
