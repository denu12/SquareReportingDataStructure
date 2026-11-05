#include "mortonplus3d.hpp"
#include <iostream>
#include <thread>
#include <atomic>
using namespace mortonplus3d;
std::uint64_t state=123456789,checks=0;
std::uint32_t rng(){state^=state<<13;state^=state>>7;state^=state<<17;return state>>32;}
void require(bool ok){++checks;if(!ok)throw std::runtime_error("failure at "+std::to_string(checks));}
Wide oracle(Point p,Point c){Wide sum=0;for(unsigned a=0;a<3;++a){auto d=std::int64_t(p[a])-c[a];auto v=std::uint64_t(d<0?-d:d);sum+=Wide(v)*v;}return sum;}
Wide key_oracle(Point p){Wide k=0;for(unsigned a=0;a<3;++a)for(unsigned b=0;b<32;++b)k|=Wide((p[a]>>b)&1)<<(3*b+a);return k;}
void check(const std::vector<Point>& p){
 MortonPlus tree(p);Results out;
 for(unsigned q=0;q<20;++q){
  Point c=q==0?Point{0,0,0}:q==1?Point{UINT32_MAX,UINT32_MAX,UINT32_MAX}:q==2&&!p.empty()?p.back():Point{rng(),rng(),rng()};
  const Wide d=p.empty()?0:oracle(p[q%p.size()],c);
  const std::array<Wide,15> radii{0,1,Wide(1)<<32,(Wide(1)<<64)-1,Wide(1)<<64,(Wide(1)<<64)+1,(Wide(1)<<65)-1,Wide(1)<<65,(Wide(1)<<65)+1,Wide(3)*UINT32_MAX*UINT32_MAX,~Wide(0),d?d-1:0,d,d+1,Wide(rng())*rng()};
  for(auto r:radii){
   out.clear();out.push_back({17,18,19});tree.query(c,r,out);require(out[0]==Point{17,18,19});
   std::vector<Point> expected{{17,18,19}};for(auto v:p)if(oracle(v,c)<=r)expected.push_back(v);
   std::vector<Point> got(out.begin(),out.end());std::sort(got.begin(),got.end());std::sort(expected.begin(),expected.end());require(got==expected);
  }
 }
}
int main(){
 for(unsigned i=0;i<10000;++i){Point p{rng(),rng(),rng()};const auto k=detail::code(p);require(k==key_oracle(p));for(unsigned a=0;a<3;++a)require(detail::coordinate(k,a)==p[a]);}
 for(auto n:{0,1,3,4,7,8,9,15,16,17,63,64,65,127,128,129,513,1025,4097})for(unsigned mode=0;mode<4;++mode){
  std::vector<Point> p;for(int i=0;i<n;++i){Point v{rng(),rng(),rng()};
   if(mode==1)for(auto& x:v)x=UINT32_MAX-x%32;
   if(mode==2)v={7,8,9};if(mode==3)for(auto& x:v)x&=0xff000000;p.push_back(v);
  }check(p);
 }
 for(auto n:{0,1,7,8,9,64,65,73,1000,50000}){
  std::vector<Wide> keys;for(int i=0;i<n;++i)keys.push_back(i%7?key_oracle({rng(),rng(),rng()}):detail::suffix(0));std::sort(keys.begin(),keys.end());detail::RankTree tree(keys);
  for(unsigned j=0;j<1000;++j){Wide k=j==0?detail::suffix(0):j==1?0:key_oracle({rng(),rng(),rng()});require(tree.lower(k)==std::size_t(std::lower_bound(keys.begin(),keys.end(),k)-keys.begin()));}
 }
 std::vector<Point> p(4097,{1,2,3});const MortonPlus tree(p);std::atomic<bool> ok=true;std::vector<std::thread> threads;
 for(unsigned t=0;t<4;++t)threads.emplace_back([&]{Results out;for(unsigned q=0;q<100;++q){out.clear();tree.query({1,2,3},0,out);if(out.size()!=p.size())ok=false;}});for(auto& t:threads)t.join();require(ok);
 Results a;a.push_back({9,8,7});Results b(std::move(a));require(a.empty()&&b.size()==1);Results c;c=std::move(b);c.reserve(100);require(c[0]==Point{9,8,7});
 std::cout<<"MortonPlus3D checks="<<checks<<" passed\n";
}
