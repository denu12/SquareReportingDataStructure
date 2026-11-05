#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "benchmark/circle.hh"
#include "third_party/esa2026/nanoflann.hpp"
#include "THST/QuadTree.h"
#include "THST/RTree.h"
#ifndef PARLAY_SEQUENTIAL
#define PARLAY_SEQUENTIAL
#endif
#include "cpdd/basic_point.h"
#include "cpdd/cpdd.h"

namespace sfc_dynamic_competitors {
struct Point {
  using ElementType=uint32_t;
  union {uint32_t min[2]; uint32_t max[2]; struct {uint32_t x,y;};};
  Point():x(0),y(0){} Point(uint32_t a,uint32_t b):x(a),y(b){}
  bool operator==(const Point& p)const{return x==p.x&&y==p.y;}
  explicit operator spatial::BoundingBox<uint32_t,2>()const{return {min,max};}
};
using Box=std::pair<Point,Point>;
inline uint64_t key(Point p){return (uint64_t(p.x)<<32)|p.y;}
inline bool inside(Point p,const Box&q){return p.x>=q.first.x&&p.y>=q.first.y&&p.x<=q.second.x&&p.y<=q.second.y;}

// Locally adapted dynamization of our exact-reporting SSS adapter, following
// Chan 2006 p.6: shifted Morton order stored in a randomized treap. It is not
// a recovered upstream dynamic implementation, nor the paper's ANN query.
class ChanTreap {
 struct Wide{uint64_t x,y;};
 struct Node{Point p;uint64_t priority;std::unique_ptr<Node>l,r;Point first,last;Node(Point a,uint64_t b):p(a),priority(b),first(a),last(a){}};
 using Ptr=std::unique_ptr<Node>;Ptr root;std::mt19937_64 random{12121};uint64_t shift=uint32_t(random());
 Wide wide(Point p)const{return {uint64_t(p.x)+shift,uint64_t(p.y)+shift};}
 static bool less(Wide a,Wide b){auto x=a.x^b.x,y=a.y^b.y;return (x<y&&x<(x^y))?a.y<b.y:a.x<b.x;}
 static void refresh(Ptr&n){if(n){n->first=n->l?n->l->first:n->p;n->last=n->r?n->r->last:n->p;}}
 static void left(Ptr&n){auto r=std::move(n->r);n->r=std::move(r->l);refresh(n);r->l=std::move(n);n=std::move(r);refresh(n);}
 static void right(Ptr&n){auto l=std::move(n->l);n->l=std::move(l->r);refresh(n);l->r=std::move(n);n=std::move(l);refresh(n);}
 void add(Ptr&n,Point p){if(!n){n=std::make_unique<Node>(p,random());return;}if(less(wide(p),wide(n->p))){add(n->l,p);if(n->l->priority<n->priority)right(n);}else{add(n->r,p);if(n->r->priority<n->priority)left(n);}refresh(n);}
 void del(Ptr&n,Point p){if(!n)throw std::runtime_error("Chan missing deletion");if(n->p==p){if(!n->l)n=std::move(n->r);else if(!n->r)n=std::move(n->l);else if(n->l->priority<n->r->priority){right(n);del(n->r,p);}else{left(n);del(n->l,p);}}else if(less(wide(p),wide(n->p)))del(n->l,p);else del(n->r,p);refresh(n);}
 void visit(const Node*n,const Box&q,Wide lo,Wide hi,std::vector<Point>&out)const{
  if(!n)return;auto a=wide(n->first),b=wide(n->last);unsigned bits=std::bit_width((a.x^b.x)|(a.y^b.y));uint64_t mask=(uint64_t(1)<<bits)-1,x=a.x&~mask,y=a.y&~mask;
  if(hi.x<x||hi.y<y||lo.x>(x|mask)||lo.y>(y|mask))return;
  if(inside(n->p,q))out.push_back(n->p);auto p=wide(n->p);
  if(!less(p,lo))visit(n->l.get(),q,lo,hi,out);if(!less(hi,p))visit(n->r.get(),q,lo,hi,out);
 }
 public:void insert(Point p){add(root,p);}void erase(Point p){del(root,p);}void query(const Box&q,std::vector<Point>&out)const{visit(root.get(),q,wide(q.first),wide(q.second),out);}
};

class Nanoflann {
 struct Cloud{std::vector<Point>p;size_t kdtree_get_point_count()const{return p.size();}double kdtree_get_pt(size_t i,size_t d)const{return double(d?p[i].y:p[i].x)/4294967296.0;}template<class B>bool kdtree_get_bbox(B&)const{return false;}}cloud;
 using Tree=nanoflann::KDTreeSingleIndexDynamicAdaptor<nanoflann::L2_Simple_Adaptor<double,Cloud>,Cloud,2>;
 Tree tree{2,cloud,nanoflann::KDTreeSingleIndexAdaptorParams(10)};
 std::unordered_map<uint64_t,size_t> ids;
 mutable std::vector<nanoflann::ResultItem<unsigned,double>> hits;
 public:void insert(Point p){size_t i=cloud.p.size();cloud.p.push_back(p);ids.emplace(key(p),i);tree.addPoints(i,i);}
 void erase(Point p){auto it=ids.find(key(p));tree.removePoint(it->second);ids.erase(it);}
 void query(const Box&q,std::vector<Point>&out)const{
  double c[2]={(double(q.first.x)+q.second.x)/8589934592.0,(double(q.first.y)+q.second.y)/8589934592.0};
  double dx=(double(q.second.x)-q.first.x)/8589934592.0,dy=(double(q.second.y)-q.first.y)/8589934592.0;
  double radius2=dx*dx+dy*dy;
  if(auto circle=esa_campaign::active_circle){c[0]=double(circle->x)/4294967296.0;c[1]=double(circle->y)/4294967296.0;radius2=double(circle->radius2())/18446744073709551616.0;}
  hits.clear();nanoflann::RadiusResultSet<double,unsigned>result(radius2+64*std::numeric_limits<double>::epsilon(),hits);
  tree.findNeighbors(result,c,nanoflann::SearchParameters(0,false));for(auto h:hits){auto p=cloud.p[h.first];if(esa_campaign::active_circle||inside(p,q))out.push_back(p);}
 }
};

class Kiddo {
 struct Api{
  size_t(*circle_query)(void*,uint32_t,uint32_t,double,const uint32_t**);
  void*lib;void*(*create)();void(*destroy)(void*);void(*insert)(void*,uint32_t,uint32_t);void(*erase)(void*,uint32_t,uint32_t);size_t(*query)(void*,uint32_t,uint32_t,uint32_t,uint32_t,const uint32_t**);
  Api(){const char*p=std::getenv("SFC_DYNAMIC_LIBRARY");lib=dlopen(p?p:"libsfc_dynamic.so",RTLD_NOW|RTLD_LOCAL);if(!lib)throw std::runtime_error(dlerror());
   create=(decltype(create))dlsym(lib,"kiddo_dyn_create");destroy=(decltype(destroy))dlsym(lib,"kiddo_dyn_destroy");insert=(decltype(insert))dlsym(lib,"kiddo_dyn_insert");erase=(decltype(erase))dlsym(lib,"kiddo_dyn_erase");query=(decltype(query))dlsym(lib,"kiddo_dyn_query");circle_query=(decltype(circle_query))dlsym(lib,"kiddo_dyn_circle_query");if(!create||!destroy||!insert||!erase||!query||!circle_query)throw std::runtime_error("Invalid dynamic Kiddo ABI");}
 };
 static Api&api(){static Api a;return a;}void*index=api().create();
 public:~Kiddo(){api().destroy(index);}void insert(Point p){api().insert(index,p.x,p.y);}void erase(Point p){api().erase(index,p.x,p.y);}void query(const Box&q,std::vector<Point>&out)const{const uint32_t*p;size_t n;
  if(auto c=esa_campaign::active_circle)n=api().circle_query(index,c->x,c->y,double(c->radius2())/18446744073709551616.0+64*std::numeric_limits<double>::epsilon(),&p);
  else n=api().query(index,q.first.x,q.first.y,q.second.x,q.second.y,&p);
  for(size_t i=0;i<n;++i)out.emplace_back(p[2*i],p[2*i+1]);}
};

class Pkd {
 using P=cpdd::PointType<uint64_t,2>;using Tree=cpdd::ParallelKDtree<P>;
 std::unique_ptr<Tree>tree=std::make_unique<Tree>();size_t size=0;
 static P point(Point p){return P({p.x,p.y});}
 public:void insert(Point p){tree->pointInsert(point(p),2);++size;}
 void erase(Point p){if(size==1){tree=std::make_unique<Tree>();size=0;}else{tree->pointDelete(point(p),2);--size;}}
 void query(const Box&q,std::vector<Point>&out)const{if(!size)return;auto b=std::make_pair(point(q.first),point(q.second));size_t i=0,k=0,n=tree->range_count(b,i,k);if(!n)return;parlay::sequence<P>hits(n);size_t got=tree->range_query_serial(b,parlay::make_slice(hits));for(size_t j=0;j<got;++j)out.emplace_back(hits[j].pnt[0],hits[j].pnt[1]);}
};
class ThstRtree {
 spatial::RTree<uint32_t,Point,2>tree;
 public:void insert(Point p){tree.insert(p);}void erase(Point p){tree.remove(p);}void query(const Box&q,std::vector<Point>&out)const{tree.query(spatial::within<2>(q.first.min,q.second.max),std::back_inserter(out));}
};
class ThstQuad {
 static constexpr uint32_t lo[2]={0,0},hi[2]={UINT32_MAX,UINT32_MAX};spatial::QuadTree<uint32_t,Point,2>tree{lo,hi};
 public:void insert(Point p){tree.insert(p);}void erase(Point p){tree.remove(p);}void query(const Box&q,std::vector<Point>&out)const{tree.query(spatial::within<2>(q.first.min,q.second.max),std::back_inserter(out));}
};
// Same multiset shim for all six additions: native indexes hold distinct
// coordinates; multiplicities are expanded into actual reported output.
// This avoids upstream disagreement about removing all vs one duplicate.
template<class Backend>class Multiset {
 Backend tree;std::unordered_map<uint64_t,size_t> counts;size_t duplicate_keys=0;
 public:void insert(Point p){auto&n=counts[key(p)];if(n==1)++duplicate_keys;if(n++==0)tree.insert(p);}
 void erase(Point p){auto i=counts.find(key(p));if(i==counts.end())throw std::runtime_error("Absent point deletion");if(i->second==2)--duplicate_keys;if(--i->second==0){tree.erase(p);counts.erase(i);}}
 void query(const Box&q,std::vector<Point>&out)const{if(q.first.x>q.second.x||q.first.y>q.second.y)return;auto begin=out.size();tree.query(q,out);if(!duplicate_keys)return;auto end=out.size();for(size_t j=begin;j<end;++j){auto p=out[j];auto i=counts.find(key(p));if(i==counts.end())throw std::runtime_error("Deleted point reported");out.insert(out.end(),i->second-1,p);}}
};
}
