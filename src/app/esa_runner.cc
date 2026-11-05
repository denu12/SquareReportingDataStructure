#include <iomanip>
#include <iostream>
#include "benchmark/esa_campaign.hh"
int main(int argc,char**argv){
  using namespace esa_campaign;
  try{
    if(argc==2 && std::string(argv[1])=="--list"){for(const auto& [name,run]:registry())std::cout<<"ESA_ALGORITHM "<<name<<"\n";return 0;}
    if(argc!=5){std::cerr<<"usage: esa_runner ALGORITHM DATA verify|bench RESULT.json\n";return 2;}
    if(!registry().contains(argv[1]))throw std::runtime_error("Unregistered ESA algorithm");
    std::string mode=argv[3];if(mode!="verify"&&mode!="bench")throw std::runtime_error("Invalid mode");
    auto data=load(argv[2]);auto r=registry().at(argv[1])(data,mode=="verify");
    std::ofstream out(argv[4]);out<<std::setprecision(17)<<"{\"algorithm\":\""<<argv[1]<<"\",\"mode\":\""<<mode<<"\",\"status\":\""<<(r.correct?"success":"incorrect")<<"\",\"build_seconds\":"<<r.build_seconds<<",\"query_seconds\":"<<r.query_seconds<<",\"update_seconds\":"<<r.update_seconds<<",\"insertions\":"<<r.insertions<<",\"deletions\":"<<r.deletions<<",\"queries\":"<<r.queries<<",\"answers\":"<<r.answers<<",\"failed_query\":"<<r.failed_query<<",\"output_contract\":\""<<r.output_contract<<"\",\"membership_contract\":\""<<r.membership_contract<<"\",\"integer_mismatch_queries\":"<<r.integer_mismatch_queries<<"}\n";
    if(!out)throw std::runtime_error("Could not write result");return r.correct?0:3;
  }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}
