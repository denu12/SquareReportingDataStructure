#include "morton3D.cc"
#include "mortonSIMD3D.hpp"
#include "mortonplus3d.hpp"
#include <random>
#include <iostream>
#include <iterator>
using Point=std::array<uint32_t,3>;
using Wide=unsigned __int128;
Wide distance(Point p,Point c){Wide sum=0;for(unsigned a=0;a<3;++a){__int128 d=__int128(p[a])-c[a];sum+=Wide(d*d);}return sum;}
void require(bool ok){if(!ok)throw std::runtime_error("3D correctness failure");}
int main(){
 std::mt19937 rng(20261015);size_t checks=0;
 for(unsigned count=1;count<=8;++count)for(unsigned mask=0;mask<256;++mask){
  std::vector<Point> p(count);std::vector<Point> expected;
  for(unsigned i=0;i<count;++i){p[i]=(mask>>i&1)?Point{i,i+1,i+2}:Point{100+i,100+i,100+i};if(mask>>i&1)expected.push_back(p[i]);}
  for(unsigned variant=0;variant<2;++variant){
   std::array<Point,13> out;out.fill({77,88,99});unsigned selected,n;
   if(variant==0){alignas(32) uint32_t axes[3][8]{};for(unsigned j=0;j<count;++j)for(unsigned a=0;a<3;++a)axes[a][j]=p[j][a];__m256i v[3];for(unsigned a=0;a<3;++a)v[a]=_mm256_load_si256(reinterpret_cast<const __m256i*>(axes[a]));selected=mask&((1u<<count)-1);n=diskreport::morton3d::simd_detail::append_axes(out.data()+1,v,selected);}
   else {mortonplus3d::detail::Batch8 b(p.data(),count);selected=b.mask({0,0,0},1000);n=b.append(out.data()+1,selected);}
   require(selected==(mask&((1u<<count)-1)));require(n==expected.size());
   require(std::equal(expected.begin(),expected.end(),out.begin()+1));require(out.front()==Point{77,88,99}&&out.back()==Point{77,88,99});++checks;
  }
 }
 for(unsigned n:{0,1,7,8,9,15,16,17,31,32,33,63,64,65,127,128,129,511,512,513,4097})for(unsigned kind=0;kind<4;++kind){
  std::vector<Point> p(n);for(auto& v:p)for(auto& x:v){x=rng();if(kind==1)x&=0xff000000;if(kind==2)x=UINT32_MAX-(x%16);if(kind==3)x=7;}
  if(n>2){p[0]=p[1]={0,0,0};p[2]={UINT32_MAX,UINT32_MAX,UINT32_MAX};}
  diskreport::morton3d::Morton3D plain(p);diskreport::morton3d::MortonSIMD3D simd(p);mortonplus3d::MortonPlus plus(p);
  for(unsigned q=0;q<12;++q){Point c=q==0?Point{0,0,0}:q==1?Point{UINT32_MAX,UINT32_MAX,UINT32_MAX}:q==2&&n?p.back():Point{uint32_t(rng()),uint32_t(rng()),uint32_t(rng())};
   const Wide d=n?distance(p[q%n],c):0;
   for(Wide radius:{Wide(0),Wide(1),Wide(25),(Wide(1)<<64)-1,Wide(1)<<64,(Wide(1)<<64)+1,(Wide(1)<<65)-1,Wide(1)<<65,(Wide(1)<<65)+1,Wide(3)*UINT32_MAX*UINT32_MAX,~Wide(0),d?d-1:0,d,d+1,Wide(rng())*rng()}){
    std::vector<Point> expected{{17,18,19}},actual=expected;diskreport::morton3d::Results so;so.push_back(expected[0]);mortonplus3d::Results po;po.push_back(expected[0]);
    for(auto v:p)if(distance(v,c)<=radius)expected.push_back(v);std::sort(expected.begin(),expected.end());
    plain.query_squared(c,radius,std::back_inserter(actual));simd.query_squared(c,radius,so);plus.query(c,radius,po);
    std::sort(actual.begin(),actual.end());std::vector<Point> sv(so.begin(),so.end()),pv(po.begin(),po.end());std::sort(sv.begin(),sv.end());std::sort(pv.begin(),pv.end());
    require(actual==expected&&sv==expected&&pv==expected);++checks;
   }
  }
 }
 std::cout<<checks<<" 3D geometry and compaction cases passed for all three indexes\n";
}
