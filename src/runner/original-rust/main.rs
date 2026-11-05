#![allow(dead_code)]
#[path="../../third_party/esa2026-workloads/src/dvec.rs"] pub mod dvec;
#[path="../../third_party/esa2026/src/query.rs"] pub mod query;
#[path="../../third_party/esa2026-workloads/src/kiddo.rs"] pub mod kiddo;
#[path="../../third_party/esa2026-workloads/src/nabo.rs"] pub mod nabo;
#[path="../../third_party/esa2026-workloads/src/neighbourhood.rs"] pub mod neighbourhood;
#[path="../../third_party/esa2026-workloads/src/vptree.rs"] pub mod vptree;
#[path="../../third_party/esa2026-workloads/src/orthtree.rs"] pub mod orthtree;
#[path="../../third_party/esa2026-workloads/src/grid.rs"] pub mod grid;
#[path="../../third_party/esa2026-workloads/src/snn.rs"] pub mod snn;
pub use query::Query;
pub type NodeId=usize;
pub mod graph {pub struct Graph;impl Graph {pub fn is_connected(&self,_a:usize,_b:usize)->bool{false}pub fn neighbors(&self,_i:usize)->&[usize]{&[]}pub fn weight(&self,_i:usize)->f64{1.0}}}
#[derive(Clone)]pub struct Embedding<'a,const D:usize>{pub positions:Vec<dvec::DVec<D>>,pub graph:&'a graph::Graph}
static GRAPH:graph::Graph=graph::Graph;
use std::io::Read;
fn u32_read(f:&mut impl Read)->u32{let mut b=[0;4];f.read_exact(&mut b).unwrap();u32::from_le_bytes(b)}
fn u64_read(f:&mut impl Read)->u64{let mut b=[0;8];f.read_exact(&mut b).unwrap();u64::from_le_bytes(b)}
fn run<const D:usize>(f:&mut impl Read,kind:&str,bench:bool){
 let n=u64_read(f) as usize;let q=u64_read(f) as usize;
 let points:Vec<[u32;D]>=(0..n).map(|_|std::array::from_fn(|_|u32_read(f))).collect();
 let queries:Vec<([u32;D],u128)>=(0..q).map(|_|{let c=std::array::from_fn(|_|u32_read(f));let low=u64_read(f) as u128;(c,low|((u64_read(f) as u128)<<64))}).collect();
 let start=std::time::Instant::now();
 let e=Embedding{positions:points.iter().map(|p|dvec::DVec::new(p.map(|x|(x as f64/4294967296.0) as f32))).collect(),graph:&GRAPH};
 let index:Option<Box<dyn Query<D>>>=if n==0{None}else{Some(match kind {
 "esa_kiddo"=>Box::new(kiddo::Kiddo::new(e)),
 "esa_nabo"=>Box::new(nabo::Nabo::new(e)),
 "esa_neighbourhood"=>Box::new(neighbourhood::Neihbourhood::new(e)),
 "esa_vptree"=>Box::new(vptree::VPTree::new(e)),
 "esa_orthtree"=>Box::new(orthtree::Orthtree::new(e)),
 "esa_grid"=>Box::new(grid::Grid::new(e)),
 "esa_snn_rust"=>Box::new(snn::Snn::new(&e)),
 _=>panic!("Unknown original implementation")})};
 let build_seconds=start.elapsed().as_secs_f64();let mut ids=Vec::new();let mut answers=0;let mut query_seconds=0.0;
 let batch=std::time::Instant::now();
 for (j,(center,r2)) in queries.iter().enumerate(){
  let start=std::time::Instant::now();ids.clear();
  if let Some(t)=&index{t.query_radius(dvec::DVec::new(center.map(|x|(x as f64/4294967296.0) as f32)),(*r2 as f64).sqrt()/4294967296.0,&mut ids);}
  std::hint::black_box(&ids);answers+=ids.len();if !bench{query_seconds+=start.elapsed().as_secs_f64();}
  if !bench{
   let expected:Vec<usize>=points.iter().enumerate().filter(|(_,p)|p.iter().zip(center).map(|(&x,&y)|{let d=(x as i64-y as i64).unsigned_abs() as u128;d*d}).sum::<u128>()<=*r2).map(|(i,_)|i).collect();
   ids.sort_unstable();if ids!=expected{println!("{{\"status\":\"incorrect\",\"reason\":\"original API differs from exact integer range query\",\"query\":{},\"actual_count\":{},\"expected_count\":{}}}",j,ids.len(),expected.len());std::process::exit(2);}
  }
 }
 if bench{query_seconds=batch.elapsed().as_secs_f64();}
 println!("{{\"status\":\"success\",\"queries\":{},\"answers\":{},\"build_seconds\":{},\"query_seconds\":{},\"output_contract\":\"point_ids_only\",\"membership_contract\":\"original_f32_api\"}}",q,answers,build_seconds,query_seconds);
}
fn main(){let args:Vec<_>=std::env::args().collect();let mut f=std::fs::File::open(&args[1]).unwrap();let d=u32_read(&mut f);let bench=args.iter().any(|a|a=="--bench");match d{2=>run::<2>(&mut f,&args[2],bench),3=>run::<3>(&mut f,&args[2],bench),_=>panic!("Original campaign build supports 2D and 3D")}}
