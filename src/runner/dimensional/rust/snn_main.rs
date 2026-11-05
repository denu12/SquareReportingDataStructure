#![allow(dead_code)]
// Original Appendix G SNN source; f32 is retained rather than repaired.
pub mod dvec {#[derive(Clone,Copy)]pub struct DVec<const D:usize>{pub components:[f32;D]}impl<const D:usize>DVec<D>{pub fn new(components:[f32;D])->Self{Self{components}}}}
#[path="../../../third_party/esa2026/src/query.rs"] pub mod query;
#[path="../../../third_party/esa2026-workloads/src/snn.rs"] pub mod snn;
pub use query::Query;
pub type NodeId=usize;
pub mod graph { pub struct Graph; impl Graph {pub fn is_connected(&self,_a:usize,_b:usize)->bool{false}pub fn neighbors(&self,_i:usize)->&[usize]{&[]}pub fn weight(&self,_i:usize)->f64{1.0}} }
#[derive(Clone)]pub struct Embedding<'a,const D:usize>{pub positions:Vec<dvec::DVec<D>>,pub graph:&'a graph::Graph}
static GRAPH:graph::Graph=graph::Graph;
fn engine<const D:usize>(p:&[[u32;D]],_kind:&str,_radius_hint:f64)->Box<dyn Query<D>>{
 let e=Embedding{positions:p.iter().map(|p|query_point(*p)).collect(),graph:&GRAPH};
 Box::new(snn::Snn::new(&e))
}
fn query_point<const D:usize>(c:[u32;D])->dvec::DVec<D>{dvec::DVec::new(c.map(|x|(x as f64/4294967296.0) as f32))}
const IDS_ONLY:bool=false;
fn search_radius<const D:usize>(r2:u128,_kind:&str)->f64{
 let r=sprk::integer_reporting::candidate_radius::<D>((r2 as f64).sqrt()/4294967296.0) as f64;
 (r*r+32.0*(D*D) as f64*f32::EPSILON as f64).sqrt().next_up()
}
include!("driver.rs");
