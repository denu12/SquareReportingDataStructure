#![allow(dead_code)]
#[path="../../../third_party/esa2026/src/dvec.rs"] pub mod dvec;
#[path="../../../third_party/esa2026/src/query.rs"] pub mod query;
pub use query::Query;
pub type NodeId=usize;
pub mod graph { pub struct Graph; impl Graph {pub fn is_connected(&self,_a:usize,_b:usize)->bool{false}pub fn neighbors(&self,_i:usize)->&[usize]{&[]}pub fn weight(&self,_i:usize)->f64{1.0}} }
#[derive(Clone)] pub struct Embedding<'a,const D:usize>{pub positions:Vec<dvec::DVec<D>>,pub graph:&'a graph::Graph}
static GRAPH:graph::Graph=graph::Graph;
struct Spark<const D:usize>(sprk::Sprk<D,8,f64,u32>);
impl<const D:usize> Query<D> for Spark<D>{fn query_radius(&self,p:dvec::DVec<D>,r:f64,out:&mut Vec<usize>){self.0.query_radius(&p.components,r,out);}}
fn engine<const D:usize>(p:&[[u32;D]],kind:&str,_radius_hint:f64)->Box<dyn Query<D>> {
 let coords:Vec<[f64;D]>=p.iter().map(|p|p.map(|v|v as f64/4294967296.0)).collect();
 assert_eq!(kind,"esa_sprk","Unknown original SPRK backend");
 Box::new(Spark(sprk::Sprk::new(&coords)))
}
fn query_point<const D:usize>(c:[u32;D])->dvec::DVec<D>{dvec::DVec::new(c.map(|x|x as f64/4294967296.0))}
const IDS_ONLY:bool=true;
fn search_radius<const D:usize>(r2:u128,kind:&str)->f64{
 let r=(r2 as f64).sqrt()/4294967296.0;
 if kind=="esa_sprk" {(r2 as f64/18446744073709551616.0+64.0*f64::EPSILON).sqrt()}
 else {((r*r*(1.0+64.0*D as f64*f64::EPSILON)).next_up().sqrt()).next_up()}
}
include!("driver.rs");
