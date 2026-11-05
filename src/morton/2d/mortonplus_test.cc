#include "mortonplus.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
using namespace mortonplus;
std::uint64_t state=0x123456789abc;
std::uint32_t random32(){state^=state<<13;state^=state>>7;state^=state<<17;return state>>32;}
Wide distance(Point a,Point b){Wide s=0;for(unsigned i=0;i<2;++i){const std::int64_t d=std::int64_t(a[i])-b[i];s+=Wide(d<0?-d:d)*Wide(d<0?-d:d);}return s;}
std::size_t checks=0;
void require(bool ok){if(!ok)throw std::runtime_error("test failed at check "+std::to_string(checks));++checks;}
void check(const std::vector<Point>& points){
 MortonPlus tree(points);std::vector<Point> out;Results fast;
 for(unsigned q=0;q<4;++q){
  Point c=q==0?Point{0,0}:q==1?Point{UINT32_MAX,UINT32_MAX}:q==2&&!points.empty()?points[points.size()/2]:Point{random32(),random32()};
  const Wide d=points.empty()?0:distance(points.back(),c);
  std::vector<Wide> radii{0,1,(Wide(1)<<64)-1,Wide(1)<<64,(Wide(1)<<64)+1,Wide(2)*UINT32_MAX*UINT32_MAX,~Wide(0),d?d-1:0,d,d+1};
  for(auto r:radii){
   std::vector<Point> expected{{77,88}};for(auto p:points)if(distance(p,c)<=r)expected.push_back(p);std::sort(expected.begin(),expected.end());
   {
    fast.clear();fast.push_back({77,88});tree.query(c,r,fast);require(fast[0]==Point{77,88});out.assign(fast.begin(),fast.end());std::sort(out.begin(),out.end());require(out==expected);
   }
  }
 }
}
template<class T>T read(std::istream& f){T x{};if(!f.read(reinterpret_cast<char*>(&x),sizeof x))throw std::runtime_error("bad fixture");return x;}
void fixture(const char* path,const char* output){
 std::ifstream f(path,std::ios::binary);std::ofstream g(output,std::ios::binary);
 for(auto count=read<std::uint64_t>(f);count--;){
  const auto n=read<std::uint64_t>(f),q=read<std::uint64_t>(f);std::vector<Point> points(n);
  for(auto& p:points){p[0]=read<std::uint32_t>(f);p[1]=read<std::uint32_t>(f);}
  MortonPlus tree(points);Results out;
  for(std::uint64_t j=0;j<q;++j){Point c{read<std::uint32_t>(f),read<std::uint32_t>(f)};auto r=read<Wide>(f);out.clear();tree.query(c,r,out);
   std::vector<Point> v(out.begin(),out.end());std::sort(v.begin(),v.end());std::uint64_t len=v.size();g.write(reinterpret_cast<char*>(&len),8);if(len)g.write(reinterpret_cast<char*>(v.data()),len*8);
  }
 }
}
int main(int argc,char** argv){
 if(argc==3){fixture(argv[1],argv[2]);return 0;}
 // Every acceptance mask, with guards around the writable SIMD batch.
 std::array<Point,CHUNK> lanes{};for(unsigned i=0;i<CHUNK;++i)lanes[i]={11+i*10,12+i*10};
 for (unsigned mask=0; mask<(1u<<CHUNK); ++mask) {
  std::array<Point,CHUNK+2> packed;packed.fill(Point{99,98});
  const unsigned count=detail::emit(detail::load(lanes.data()),mask,packed.data()+1);
  require(count==unsigned(std::popcount(mask)));
  require(packed.front()==Point{99,98} && packed.back()==Point{99,98});
  unsigned j=1;for(unsigned i=0;i<CHUNK;++i)if(mask&(1u<<i))require(packed[j++]==lanes[i]);
 }
 for(auto n:{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,31,32,33,73,127,128,129,255,256,257,511,512,513,1023,1024,1025,1100})for(unsigned kind=0;kind<4;++kind){
  std::vector<Point> p;for(int i=0;i<n;++i){Point v{random32(),random32()};if(kind==1)v={UINT32_MAX-v[0]%64,v[1]%64};if(kind==2)v={123456,654321};if(kind==3)v={v[0]&0xff000000,v[1]&0xff000000};p.push_back(v);}
  check(p);
 }
 for(auto n:{0,1,7,8,9,63,64,65,72,73,80,81,1000,4097,50000}){
  std::vector<std::uint64_t> keys;for(int i=0;i<n;++i)keys.push_back(i%7==0?UINT64_MAX:std::uint64_t(random32())<<32|random32());std::sort(keys.begin(),keys.end());
  detail::RankTree tree(keys);for(unsigned j=0;j<1000;++j){const auto x=j==0?UINT64_MAX:j==1?0:std::uint64_t(random32())<<32|random32();require(tree.lower(x)==std::size_t(std::lower_bound(keys.begin(),keys.end(),x)-keys.begin()));}
 }
 std::vector<Point> p(2000,{123,456});const MortonPlus tree(p);std::atomic<bool> ok=true;
 std::vector<std::thread> threads;for(unsigned t=0;t<4;++t)threads.emplace_back([&]{Results r;for(unsigned j=0;j<100;++j){r.clear();tree.query({123,456},0,r);if(r.size()!=p.size())ok=false;}});for(auto& t:threads)t.join();require(ok);
 Results a;a.push_back({9,8});Results b(std::move(a));require(a.empty()&&b.size()==1&&b[0]==Point{9,8});Results c;c=std::move(b);require(c.size()==1);c.reserve(100);require(c[0]==Point{9,8});
 std::cout<<"kernel="<<KERNEL<<" checks="<<checks<<" all passed\n";
}
