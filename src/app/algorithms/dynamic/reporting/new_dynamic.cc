#include "app/algorithms/dynamic/reporting/new_dynamic.hh"
#include "app/algorithms/dynamic/reporting/adapter.hh"
namespace diskreport::app::algorithms::dynamic::sfc {
template<class Backend,typename T>struct NewDynamic {
 using PointType=sfc_dynamic_competitors::Point;static constexpr int offset=0;
 struct Type{
  sfc_dynamic_competitors::Multiset<Backend>index;size_t reported=0;
  template<class A,class B>Type(const A&,const B&){}
  void handle_insertion(PointType p){index.insert(p);}void handle_deletion(PointType p){index.erase(p);}
  void esa_query(PointType lo,PointType hi,std::vector<PointType>&out){index.query({lo,hi},out);}
  bool handle_query(std::pair<PointType,PointType>q){std::vector<PointType>out;index.query(q,out);reported+=out.size();return true;}
  bool valid(){return true;}size_t size(){return reported;}
 };
};
#define REGISTER_DYNAMIC(TYPE,NAME,ID) \
 template<class T>using ID##Impl=NewDynamic<sfc_dynamic_competitors::TYPE,T>; \
 using ID=DynSFCInMemory<ID##Impl,uint32_t>;REGISTER_IMPL_NAMED(ID,NAME)
REGISTER_DYNAMIC(ChanTreap,"chan_sss_dyn_ADAPTED_DYNAMIC",ChanDyn);
REGISTER_DYNAMIC(Kiddo,"kiddo_mutable_dyn_UPSTREAM",KiddoDyn);
REGISTER_DYNAMIC(Nanoflann,"nanoflann_dyn_UPSTREAM",NanoDyn);
REGISTER_DYNAMIC(Pkd,"pkd_dyn_UPSTREAM",PkdDyn);
REGISTER_DYNAMIC(ThstRtree,"thst_rtree_dyn_UPSTREAM",ThstRDyn);
REGISTER_DYNAMIC(ThstQuad,"thst_quad_dyn_UPSTREAM",ThstQDyn);
}
