#include "morton.cc"
#include "mortonplus.hpp"
#include "mortonSIMD.cc"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <iterator>
#include <string>
#include <sstream>
using Wide=unsigned __int128;
template<class T>T read(std::istream& in){T x{};if(!in.read(reinterpret_cast<char*>(&x),sizeof x))throw std::runtime_error("Truncated input");return x;}
template<class P> P point(std::uint32_t x,std::uint32_t y){return P{x,y};}
template<class P>std::uint64_t key(const P& p){if constexpr(requires{p.x;})return std::uint64_t(p.x)<<32|p.y;else return std::uint64_t(p[0])<<32|p[1];}
template<class P>bool contains(const P& p,std::array<std::uint32_t,2> c,Wide r){const auto k=key(p);const __int128 dx=__int128(k>>32)-c[0],dy=__int128(std::uint32_t(k))-c[1];return Wide(dx*dx+dy*dy)<=r;}
struct Query{std::array<std::uint32_t,2> c;Wide r;};
template<class Index,class Point,class Output>int run(std::istream& f,std::uint64_t n,std::uint64_t q,bool bench,const std::string& algorithm,const std::string& dest){
 std::vector<Point> points;points.reserve(n);
 for(std::uint64_t i=0;i<n;++i){auto x=read<std::uint32_t>(f);auto y=read<std::uint32_t>(f);points.push_back(point<Point>(x,y));}
 std::vector<Query> queries;queries.reserve(q);for(std::uint64_t i=0;i<q;++i){auto x=read<std::uint32_t>(f),y=read<std::uint32_t>(f);auto lo=read<std::uint64_t>(f),hi=read<std::uint64_t>(f);queries.push_back({{x,y},Wide(lo)|Wide(hi)<<64});}
 if(f.peek()!=EOF)throw std::runtime_error("Trailing input data");
 using Clock=std::chrono::steady_clock;auto start=Clock::now();Index index(points);auto build=std::chrono::duration<double>(Clock::now()-start).count();
 Output out;std::uint64_t answers=0,completed=0;double elapsed=0;long long failed=-1;auto batch=Clock::now();
 for(std::size_t j=0;j<queries.size();++j){const auto [c,r]=queries[j];if(!bench)start=Clock::now();out.clear();
  if constexpr(std::is_same_v<Index,mortonplus::MortonPlus>)index.query(c,r,out);
  else if constexpr(std::is_same_v<Index,diskreport::morton_Morton::Morton>)index.query(diskreport::morton_Morton::Circle{{c[0],c[1]},r},std::back_inserter(out));
  else if constexpr(std::is_same_v<Index,diskreport::morton_MortonSIMD::MortonSIMD>)index.query(diskreport::morton_MortonSIMD::Circle{{c[0],c[1]},r},out);
  else index.query(diskreport::morton_MortonSIMD::Circle{{c[0],c[1]},r},std::back_inserter(out));
  // Compiler barrier exposes every materialized coordinate, not just the count.
  asm volatile("" : : "r"(out.data()),"r"(out.size()) : "memory");answers+=out.size();++completed;
  if(!bench){elapsed+=std::chrono::duration<double>(Clock::now()-start).count();std::vector<std::uint64_t> actual,expected;for(auto p:out)actual.push_back(key(p));for(auto p:points)if(contains(p,c,r))expected.push_back(key(p));std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());if(actual!=expected){failed=j;break;}}
 }
 if(bench)elapsed=std::chrono::duration<double>(Clock::now()-batch).count();
 std::ostringstream s;s<<std::setprecision(17)<<"{\"status\":\""<<(failed<0?"success":"incorrect")<<"\",\"algorithm\":\""<<algorithm<<"\",\"dimension\":2,\"queries\":"<<completed<<",\"answers\":"<<answers<<",\"build_seconds\":"<<build<<",\"query_seconds\":"<<elapsed<<",\"update_seconds\":0,\"insertions\":0,\"deletions\":0,\"output_contract\":\"integer_points\",\"membership_contract\":\"closed_integer_ball\",\"failed_query\":"<<failed<<"}";
 if(!dest.empty()){std::ofstream g(dest);g<<s.str()<<'\n';}std::cout<<s.str()<<'\n';return failed<0?0:2;
}
int main(int argc,char** argv){try{
 std::string path,algorithm,dest;bool bench=false;
 if(argc==5){algorithm=argv[1];path=argv[2];std::string mode=argv[3];if(mode!="bench"&&mode!="verify")throw std::runtime_error("Bad mode");bench=mode=="bench";dest=argv[4];}
 else if(argc==3||argc==4){path=argv[1];algorithm=argv[2];if(argc==4&&std::string(argv[3])!="--bench")throw std::runtime_error("Bad mode");bench=argc==4;}
 else throw std::runtime_error("Bad arguments");
 std::ifstream f(path,std::ios::binary);char magic[8];f.read(magic,8);if(!f)throw std::runtime_error("Bad header");if(std::string(magic,8)!=std::string("ESA2D01\0",8)){f.seekg(0);if(read<std::uint32_t>(f)!=2)throw std::runtime_error("Unsupported dimension");}
 auto n=read<std::uint64_t>(f),q=read<std::uint64_t>(f);if(n>1000000000||q>1000000000)throw std::runtime_error("Bad counts");
 if(algorithm=="MortonPlusCPP")return run<mortonplus::MortonPlus,mortonplus::Point,mortonplus::Results>(f,n,q,bench,algorithm,dest);
 if(algorithm=="Morton")return run<diskreport::morton_Morton::Morton,diskreport::morton_Morton::Point,std::vector<diskreport::morton_Morton::Point>>(f,n,q,bench,algorithm,dest);
 if(algorithm=="MortonSIMD")return run<diskreport::morton_MortonSIMD::MortonSIMD,diskreport::morton_MortonSIMD::Point,diskreport::morton_MortonSIMD::Results>(f,n,q,bench,algorithm,dest);
 throw std::runtime_error("Unknown algorithm");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
