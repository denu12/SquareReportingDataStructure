// Dimensional interfaces. Reporting adaptations are documented in docs/REPORTING-ADAPTATIONS.md.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <unordered_map>
#include <memory>
#include <limits>
#include <numeric>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
using Wide=unsigned __int128;
using Point=std::vector<std::uint32_t>;
template<class T> T read(std::istream& f){T x{};if(!f.read((char*)&x,sizeof x))throw std::runtime_error("Truncated input");return x;}
struct Data {
 unsigned d; std::vector<Point> points,centers;std::vector<Wide> radii;
 explicit Data(const char* path){std::ifstream f(path,std::ios::binary);d=read<unsigned>(f);
 auto n=read<std::uint64_t>(f),q=read<std::uint64_t>(f);
 for(std::size_t i=0;i<n+q;++i){Point p(d);for(auto& x:p)x=read<std::uint32_t>(f);
 if(i<n)points.push_back(p);else{centers.push_back(p);Wide r=read<std::uint64_t>(f);r|=Wide(read<std::uint64_t>(f))<<64;radii.push_back(r);}}
 }
 bool contains(std::size_t id,std::size_t q)const{Wide sum=0;for(unsigned a=0;a<d;++a){auto diff=std::int64_t(points[id][a])-centers[q][a];auto v=std::uint64_t(diff<0?-diff:diff);sum+=Wide(v)*v;}return sum<=radii[q];}
 double radius(std::size_t q)const{
  return std::sqrt(double(radii[q]))/4294967296.;
 }
 std::vector<double> center(std::size_t q)const{std::vector<double> v;for(auto x:centers[q])v.push_back(double(x)/4294967296.);return v;}
};
struct Query {std::function<void(std::size_t)> report;std::function<std::size_t()> size;std::function<bool(std::size_t)> correct;};
bool verify_ids(const Data& d,std::size_t q,std::vector<std::size_t> ids){std::vector<std::size_t> expected;for(std::size_t i=0;i<d.points.size();++i)if(d.contains(i,q))expected.push_back(i);std::sort(ids.begin(),ids.end());return ids==expected;}
#if defined(BACKEND_BOOST)
const char* output_contract="integer_points";const char* output_representation="native_boost_points";const char* membership_contract="closed_integer_ball";
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
namespace bg=boost::geometry;namespace bi=bg::index;
template<unsigned D,class Policy> Query boost_index(Data& data){
 using P=bg::model::point<std::uint32_t,D,bg::cs::cartesian>;using Tree=bi::rtree<P,Policy>;
 auto point=[](const Point& p){P result;[&]<std::size_t... A>(std::index_sequence<A...>){(bg::set<A>(result,p[A]),...);}(std::make_index_sequence<D>{});return result;};
 std::vector<P> entries;entries.reserve(data.points.size());for(const auto& p:data.points)entries.push_back(point(p));
 auto tree=std::make_shared<Tree>(entries.begin(),entries.end());
 auto hits=std::make_shared<std::vector<P>>();
 auto report=[&,tree,point,hits](std::size_t q){hits->clear();std::uint64_t low=0,high=UINT64_MAX;while(low<high){auto m=low+(high-low)/2;if(Wide(m)*m<data.radii[q])low=m+1;else high=m;}
 Point lo(D),hi(D);for(unsigned a=0;a<D;++a){auto c=data.centers[q][a];lo[a]=low>c?0:c-low;hi[a]=low>UINT32_MAX-c?UINT32_MAX:c+low;}
 auto inside=[&](const P& p){Wide sum=0;
  [&]<std::size_t... A>(std::index_sequence<A...>){
   auto add=[&](auto axis){auto delta=std::int64_t(bg::get<decltype(axis)::value>(p))-data.centers[q][axis];auto magnitude=std::uint64_t(delta<0?-delta:delta);sum+=Wide(magnitude)*magnitude;};
   (add(std::integral_constant<std::size_t,A>{}),...);
  }(std::make_index_sequence<D>{});return sum<=data.radii[q];};
 auto predicate=bi::intersects(bg::model::box<P>(point(lo),point(hi))) && bi::satisfies(inside);
 tree->query(predicate,std::back_inserter(*hits));
 asm volatile("" : : "r"(hits->data()), "r"(hits->size()) : "memory");
 };
 auto correct=[&,hits](std::size_t q){
  // Conversion is for verification only, after the query timer stops.
  std::vector<Point> actual,expected;
  for(const auto& p:*hits){Point value(D);
   [&]<std::size_t... A>(std::index_sequence<A...>){((value[A]=bg::get<A>(p)),...);}(std::make_index_sequence<D>{});
   actual.push_back(std::move(value));
  }
  for(std::size_t i=0;i<data.points.size();++i)if(data.contains(i,q))expected.push_back(data.points[i]);
  std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());
  return actual==expected;
 };
 return {report,[hits]{return hits->size();},correct};
}
template<unsigned D>Query select_boost(Data& d,const std::string& a){if(a=="boost_lin")return boost_index<D,bi::linear<16>>(d);if(a=="boost_quad")return boost_index<D,bi::quadratic<16>>(d);return boost_index<D,bi::rstar<16>>(d);}
Query create(Data& d,const std::string& a){switch(d.d){
#define DIM(N) case N:return select_boost<N>(d,a);
DIM(2) DIM(3) DIM(4) DIM(5) DIM(6) DIM(7) DIM(8) DIM(9) DIM(10) DIM(11) DIM(12) DIM(13) DIM(14) DIM(15) DIM(16) DIM(32)
#undef DIM
default:throw std::runtime_error("Unsupported dimension");}}
#elif defined(BACKEND_ANN)
#include <ANN/ANN.h>
const char* output_contract="point_ids_only";const char* output_representation="native_ann_indices";const char* membership_contract="original_f64_api";
Query create(Data& d,const std::string&){
 auto coords=std::make_shared<std::vector<std::vector<double>>>();for(auto& p:d.points){std::vector<double> v;for(auto x:p)v.push_back(double(x)/4294967296.);coords->push_back(v);}
 auto rows=std::make_shared<std::vector<ANNpoint>>();for(auto& p:*coords)rows->push_back(p.data());
 auto tree=rows->empty()?std::shared_ptr<ANNkd_tree>{}:std::make_shared<ANNkd_tree>(rows->data(),int(rows->size()),int(d.d));
 auto hits=std::make_shared<std::vector<ANNidx>>();
 return {[&,coords,rows,tree,hits](std::size_t q){hits->clear();if(!tree)return;auto c=d.center(q);auto radius=d.radius(q);int n=tree->annkFRSearch(c.data(),radius*radius,0,nullptr,nullptr,0.0);hits->resize(n);if(n)tree->annkFRSearch(c.data(),radius*radius,n,hits->data(),nullptr,0.0);asm volatile(""::"r"(hits->data()),"r"(hits->size()):"memory");},[hits]{return hits->size();},[&,hits](std::size_t q){return verify_ids(d,q,{hits->begin(),hits->end()});}};
}
#elif defined(BACKEND_NANO)
#include "nanoflann.hpp"
const char* output_contract="point_ids_and_distances";const char* output_representation="native_nanoflann_hits";const char* membership_contract="original_f64_api";
struct Cloud{unsigned dimension;std::vector<double> coordinates;explicit Cloud(const Data& d):dimension(d.d){for(auto& p:d.points)for(auto x:p)coordinates.push_back(double(x)/4294967296.);}std::size_t kdtree_get_point_count()const{return coordinates.size()/dimension;}double kdtree_get_pt(std::size_t i,std::size_t a)const{return coordinates[i*dimension+a];}template<class B>bool kdtree_get_bbox(B&)const{return false;}};
Query create(Data& d,const std::string&){
 auto cloud=std::make_shared<Cloud>(d);using Tree=nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double,Cloud>,Cloud,-1,std::size_t>;
 auto tree=std::make_shared<Tree>(d.d,*cloud,nanoflann::KDTreeSingleIndexAdaptorParams(10));auto hits=std::make_shared<std::vector<nanoflann::ResultItem<std::size_t,double>>>();
 return {[&,cloud,tree,hits](std::size_t q){hits->clear();auto c=d.center(q);auto radius=d.radius(q);tree->radiusSearch(c.data(),radius*radius,*hits,nanoflann::SearchParameters(0,false));asm volatile(""::"r"(hits->data()),"r"(hits->size()):"memory");},[hits]{return hits->size();},[&,hits](std::size_t q){std::vector<std::size_t> ids;for(auto h:*hits)ids.push_back(h.first);return verify_ids(d,q,std::move(ids));}};
}
#elif defined(BACKEND_CGAL)
#include <CGAL/Cartesian_d.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Kd_tree.h>
#include <CGAL/Fuzzy_sphere.h>
const char* output_contract="floating_points";const char* output_representation="native_cgal_points";const char* membership_contract="original_f64_api";
Query create(Data& d,const std::string&){
 using P=CGAL::Cartesian_d<double>::Point_d;using Traits=CGAL::Search_traits_d<CGAL::Cartesian_d<double>>;using Tree=CGAL::Kd_tree<Traits>;
 std::vector<P> points;for(auto& p:d.points){std::vector<double> v;for(auto x:p)v.push_back(double(x)/4294967296.);points.emplace_back(d.d,v.begin(),v.end());}
 auto tree=std::make_shared<Tree>(points.begin(),points.end());tree->build();auto hits=std::make_shared<std::vector<P>>();
 return {[&,tree,hits](std::size_t q){hits->clear();auto c=d.center(q);tree->search(std::back_inserter(*hits),CGAL::Fuzzy_sphere<Traits>(P(d.d,c.begin(),c.end()),d.radius(q),0.));asm volatile(""::"r"(hits->data()),"r"(hits->size()):"memory");},[hits]{return hits->size();},[&,hits](std::size_t q){std::vector<Point> actual,expected;for(auto& p:*hits){Point point;for(auto it=p.cartesian_begin();it!=p.cartesian_end();++it)point.push_back(std::uint32_t(*it*4294967296.));actual.push_back(std::move(point));}for(std::size_t i=0;i<d.points.size();++i)if(d.contains(i,q))expected.push_back(d.points[i]);std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}};
}
#elif defined(BACKEND_PARGEO)
#define PARLAY_SEQUENTIAL
#include "pargeo/kdTree.h"
#include "pargeo/kdTreeRange.h"
const char* output_contract="point_ids_only";const char* output_representation="native_pargeo_indices";const char* membership_contract="original_pargeo_api";
template<int D> Query pargeo_index(Data& d){
 using P=pargeo::point<D>;using A=parlay::sequence<P>;
 auto points=std::make_shared<A>(d.points.size());for(std::size_t i=0;i<d.points.size();++i)for(int a=0;a<D;++a)(*points)[i][a]=d.points[i][a];
 auto tree=pargeo::buildKdt<D,P>(*points,false,false);
 auto hits=std::make_shared<parlay::sequence<std::size_t>>();
 return {[&,points,tree,hits](std::size_t q){P center;for(int a=0;a<D;++a)center[a]=d.centers[q][a];*hits=pargeo::kdTreeRange<D,P>(*points,tree,center,std::sqrt(double(d.radii[q])));asm volatile(""::"r"(hits->data()),"r"(hits->size()):"memory");},[hits]{return hits->size();},[&,points,hits](std::size_t q){std::vector<Point> actual,expected;for(auto id:*hits){if(id>=points->size())return false;Point p(D);for(int a=0;a<D;++a)p[a]=(*points)[id][a];actual.push_back(std::move(p));}for(std::size_t i=0;i<d.points.size();++i)if(d.contains(i,q))expected.push_back(d.points[i]);std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}};
}
Query create(Data& d,const std::string&){if(d.d==2)return pargeo_index<2>(d);if(d.d==3)return pargeo_index<3>(d);throw std::runtime_error("Unsupported dimension");}
#elif defined(BACKEND_PAM)
#define PARLAY_SEQUENTIAL
#include <pam/pam.h>
#define Point PamPoint
#include "range_utils.h"
#include "range_tree.h"
#undef Point
const char* output_contract="integer_points";const char* output_representation="native_pam_yx_pairs";const char* membership_contract="closed_integer_ball";
Query create(Data& d,const std::string&){
 if(d.d!=2)throw std::runtime_error("Original PAM example is 2D");
 using P=::PamPoint<std::int64_t,float>;using Tree=::RangeQuery<std::int64_t,float>;
 parlay::sequence<P> points(d.points.size());
 for(std::size_t i=0;i<d.points.size();++i)points[i]=P(d.points[i][0],d.points[i][1],1);
 auto tree=std::make_shared<Tree>(points);
 // The upstream map stores one entry per coordinate pair. Preserve multiplicity
 // only when needed, leaving the usual distinct-point path free of hash lookups.
 auto counts=std::make_shared<std::unordered_map<std::uint64_t,std::size_t>>();
 auto key=[](auto x,auto y){return (std::uint64_t(x)<<32)|std::uint64_t(y);};
 if(tree->range_tree.size()<d.points.size())for(const auto& p:d.points)++(*counts)[key(p[0],p[1])];
 auto hits=std::make_shared<std::vector<typename Tree::point_y>>();
 return {[&,tree,hits,counts,key](std::size_t q){
  hits->clear();
  std::uint64_t low=0,high=UINT64_MAX;
  while(low<high){auto m=low+(high-low)/2;if(Wide(m)*m<d.radii[q])low=m+1;else high=m;}
  auto lower=[&](unsigned a){return std::int64_t(low>d.centers[q][a]?0:d.centers[q][a]-low);};
  auto upper=[&](unsigned a){return std::int64_t(low>UINT32_MAX-d.centers[q][a]?UINT32_MAX:d.centers[q][a]+low);};
  auto candidates=tree->query_range(lower(0),lower(1),upper(0),upper(1));
  for(auto p:candidates){
    auto dx=p.second-d.centers[q][0],dy=p.first-d.centers[q][1];
    auto x=std::uint64_t(dx<0?-dx:dx),y=std::uint64_t(dy<0?-dy:dy);
    if(Wide(x)*x+Wide(y)*y<=d.radii[q]){
      auto count=counts->empty()?1:counts->at(key(p.second,p.first));
      hits->insert(hits->end(),count,p);
    }
  }
  asm volatile(""::"r"(hits->data()),"r"(hits->size()):"memory");
 },[hits]{return hits->size();},[&,hits](std::size_t q){std::vector<Point> actual,expected;for(auto p:*hits)actual.push_back({std::uint32_t(p.second),std::uint32_t(p.first)});for(std::size_t i=0;i<d.points.size();++i)if(d.contains(i,q))expected.push_back(d.points[i]);std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}};
}
#elif defined(BACKEND_REPORTING)
#include "reporting_adapters.h"
#elif defined(BACKEND_ADDITIONAL)
#include "additional_indexes.h"
#elif defined(BACKEND_BRUTE)
const char* output_contract="point_ids_only";const char* output_representation="indices";const char* membership_contract="closed_integer_ball";
Query create(Data& d,const std::string&){auto hits=std::make_shared<std::vector<std::size_t>>();return {[&,hits](std::size_t q){hits->clear();for(std::size_t i=0;i<d.points.size();++i)if(d.contains(i,q))hits->push_back(i);},[hits]{return hits->size();},[&,hits](std::size_t q){return verify_ids(d,q,*hits);}};}
#endif
int main(int argc,char** argv){try{
 if(argc!=3 && argc!=4)throw std::runtime_error("Usage: foreign_check CASE ALGORITHM [--bench]");
 if(argc==4 && std::string(argv[3])!="--bench")throw std::runtime_error("Unknown option");
 const bool bench=argc==4;
 const std::string algorithm=argv[2];

 Data data(argv[1]);auto start=std::chrono::steady_clock::now();
 Query query=create(data,algorithm);
 double build_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 double query_seconds=0;std::size_t answers=0;const auto batch_start=std::chrono::steady_clock::now();
 for(std::size_t q=0;q<data.centers.size();++q){
  if(!bench)start=std::chrono::steady_clock::now();
  query.report(q);
  if(!bench)query_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  answers+=query.size();
  if(!bench && !query.correct(q)){std::cout<<"{\"status\":\"incorrect\",\"query\":"<<q<<"}\n";return 2;}
 }
 if(bench)query_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-batch_start).count();
 std::cout<<"{\"status\":\"success\",\"dimension\":"<<data.d<<",\"queries\":"<<data.centers.size()
          <<",\"answers\":"<<answers<<",\"build_seconds\":"<<build_seconds<<",\"query_seconds\":"<<query_seconds
          <<",\"output_contract\":\""<<output_contract<<"\",\"output_representation\":\""<<output_representation<<"\",\"membership_contract\":\""<<membership_contract<<"\"}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
