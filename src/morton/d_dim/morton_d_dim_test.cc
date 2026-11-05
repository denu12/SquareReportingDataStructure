#include "morton_d_dim.cc"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <random>
#include <string>

using Clock=std::chrono::steady_clock;
using W=unsigned __int128;
std::size_t checks=0;
template<std::size_t D> std::vector<std::array<std::uint32_t,D>> oracle(
    const std::vector<std::array<std::uint32_t,D>>& points,
    const std::array<std::uint32_t,D>& center,W radius2) {
  std::vector<std::array<std::uint32_t,D>> result;
  for(const auto& p:points) {
    W distance=0;
    for(std::size_t a=0;a<D;++a) {
      auto delta=std::int64_t(p[a])-center[a];
      auto magnitude=std::uint64_t(delta<0?-delta:delta);
      distance+=W(magnitude)*magnitude;
    }
    if(distance<=radius2)result.push_back(p);
  }
  std::sort(result.begin(),result.end());return result;
}
template<std::size_t D> void test() {
  using I=diskreport::ddim::MortonDDim<D>;using P=typename I::Point;
  std::mt19937_64 rng(4711+D);
  // Independent bitwise codec reference: full width and every truncated depth.
  for(unsigned trial=0;trial<64;++trial) {
    P p;for(auto& x:p)x=std::uint32_t(rng());
    for(unsigned depth=0;depth<=32;++depth) {
      typename I::Key key{};
      for(std::size_t a=0;a<D;++a)for(unsigned b=32-depth;b<32;++b)
        if((p[a]>>b)&1)key[I::Codec::words-1-(D*b+a)/64]|=std::uint64_t{1}<<((D*b+a)%64);
      if(I::Codec::encode(p,depth)!=key)throw std::runtime_error("Codec mismatch");
      ++checks;
    }
  }
  for(std::size_t n: {0,1,2,7,8,9,63,64,65,71,72,73,511,512,513,647,648,649,1000,5831,5832,5833}) {
    for(unsigned layout=0;layout<4;++layout) {
      std::vector<P> points(n);
      for(std::size_t i=0;i<n;++i)for(std::size_t a=0;a<D;++a)
        points[i][a]=layout==0?std::uint32_t(rng()):layout==1?UINT32_MAX:
          layout==2?std::uint32_t((i%11)*17+a):std::uint32_t((i+a)%2?0:UINT32_MAX);
      if(n>3){points[1]=points[0];points[2]=points[0];}
      I index(points);
      auto check=[&](const P& c,W radius2,bool integer_radius=false,std::uint64_t r=0) {
        std::vector<P> out(1,P{}); // Preserve append semantics.
        if(integer_radius)index.query(c,r,std::back_inserter(out));
        else index.query_squared(c,radius2,std::back_inserter(out));
        auto expected=oracle<D>(points,c,radius2);expected.push_back(P{});
        std::sort(out.begin(),out.end());std::sort(expected.begin(),expected.end());
        if(out!=expected)throw std::runtime_error("Point multiset mismatch D="+std::to_string(D)+" n="+std::to_string(n)+" layout="+std::to_string(layout));
        ++checks;
      };
      P c{};check(c,0);check(c,1);check(c,W(D)*UINT32_MAX*UINT32_MAX);
      c.fill(UINT32_MAX);check(c,0);check(c,W(UINT32_MAX)*UINT32_MAX-1);
      check(c,W(UINT32_MAX)*UINT32_MAX);check(c,W(D)*UINT32_MAX*UINT32_MAX-1);
      check(c,~W{0});check(c,W(UINT64_MAX)*UINT64_MAX,true,UINT64_MAX);
      if(n) {check(points[0],0);check(points[n/2],0);}
      for(unsigned trial=0;trial<6;++trial) {
        for(auto& x:c)x=std::uint32_t(rng());
        const std::uint64_t r=std::uint32_t(rng());
        check(c,W(r)*r,true,r);
        if(n){W distance=0;for(std::size_t a=0;a<D;++a){auto delta=std::int64_t(points[trial%n][a])-c[a];auto mag=std::uint64_t(delta<0?-delta:delta);distance+=W(mag)*mag;}
          check(c,distance);if(distance)check(c,distance-1);check(c,distance+1);}
      }
    }
  }
  // Every axis crosses a cell boundary; streaming ordering and adjacency merges.
  std::vector<P> corner_points(33);
  for(std::size_t i=0;i<corner_points.size();++i)for(std::size_t a=0;a<D;++a)corner_points[i][a]=(1U<<31)+std::uint32_t(i+a)-16;
  I corners(corner_points);P center;center.fill(1U<<31);
  std::vector<P> out;corners.query_squared(center,10000,std::back_inserter(out));
  std::sort(out.begin(),out.end());if(out!=oracle<D>(corner_points,center,10000))throw std::runtime_error("Corner ordering mismatch");++checks;
  std::cout<<"dimension "<<D<<" passed; cumulative checks "<<checks<<'\n'<<std::flush;
}

int main() { try {
test<1>();test<2>();test<3>();test<4>();test<5>();test<6>();test<7>();test<8>();test<9>();test<10>();test<11>();test<12>();test<13>();test<14>();test<15>();test<16>();std::cout << "SUCCESS " << checks << " checks\n"; } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }}
