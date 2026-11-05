#![allow(dead_code)]
pub mod dvec; pub mod query; pub mod kiddo; pub mod nabo; pub mod neighbourhood; pub mod vptree; pub mod orthtree; pub mod grid;
pub use query::Query;
pub type NodeId=usize;
pub mod graph { pub struct Graph; impl Graph { pub fn is_connected(&self,_a:usize,_b:usize)->bool{false} pub fn neighbors(&self,_i:usize)->&[usize]{&[]} pub fn weight(&self,_i:usize)->f64{1.0} } }
#[derive(Clone)] pub struct Embedding<'a,const D:usize>{pub positions:Vec<dvec::DVec<D>>, pub graph:&'a graph::Graph}
static GRAPH:graph::Graph=graph::Graph;
use pyo3::prelude::*;
use dvec::DVec;
// Deliberately relaxed historical diagnostic: floating radius, IDs only.
struct Spark(sprk::Sprk<2,8,f64,u32>);
impl Query<2> for Spark { fn query_radius(&self,p:DVec<2>,r:f64,out:&mut Vec<usize>){ self.0.query_radius(&p.components,r,out); } }
struct PyIndex { tree:Py<PyAny>, query:Py<PyAny>, snn:bool }
impl PyIndex {
 fn new(points:&[[f64;2]],kind:u32)->Self { Python::attach(|py| {
  let np=py.import("numpy").unwrap();
  let data=np.call_method1("array",(points.to_vec(),)).unwrap();
  let snn=kind==9;
  let tree=if snn { py.import("snnpy").unwrap().getattr("build_snn_model").unwrap().call1((data,0)).unwrap() }
   else {py.import("sklearn.neighbors").unwrap().getattr(if kind==7 {"KDTree"}else{"BallTree"}).unwrap().call1((data,)).unwrap()};
  let query=if snn{np.call_method1("zeros",(2,)).unwrap()}else{np.call_method1("zeros",((1,2),)).unwrap()};
  Self{tree:tree.unbind(),query:query.unbind(),snn}
 }) }
}
impl Query<2> for PyIndex { fn query_radius(&self,p:DVec<2>,r:f64,out:&mut Vec<usize>){Python::attach(|py|{
 let arr=self.query.bind(py);
 for i in 0..2 {if self.snn{arr.set_item(i,p.components[i]).unwrap()}else{arr.set_item((0,i),p.components[i]).unwrap()}}
 let result=self.tree.bind(py).call_method1("query_radius",(arr,r)).unwrap();
 let result=if self.snn{result}else{result.get_item(0).unwrap()};
 let buffer=pyo3::buffer::PyBuffer::<i64>::get(&result).unwrap();
 if let Some(slice)=buffer.as_slice(py){out.extend(slice.iter().map(|v|usize::try_from(v.get()).unwrap()));}
 else{out.extend(buffer.to_vec(py).unwrap().into_iter().map(|v|usize::try_from(v).unwrap()));}
 });} }
struct Index { points:Vec<[u32;2]>, engine:Option<Box<dyn Query<2>>>, coordinate_engine:Option<::neighbourhood::KdTree<f64,2>>, hits:Vec<usize>, output:Vec<[u32;2]> }
#[unsafe(no_mangle)] pub unsafe extern "C" fn esa_create(kind:u32,p:*const u32,n:usize)->*mut std::ffi::c_void {
 unsafe{esa_create_with_hint(kind,p,n,0.001)}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn esa_create_with_hint(kind:u32,p:*const u32,n:usize,radius_hint:f64)->*mut std::ffi::c_void {
 let points=unsafe{std::slice::from_raw_parts(p.cast::<[u32;2]>(),n)}.to_vec();
 let coords:Vec<[f64;2]>=points.iter().map(|p|p.map(|x|x as f64/4294967296.0)).collect();
 if kind==3 && n>0 {
  let native=::neighbourhood::KdTree::new(coords);
  return Box::into_raw(Box::new(Index{points:Vec::new(),engine:None,coordinate_engine:Some(native),hits:Vec::new(),output:Vec::new()})).cast();
 }
 let e=Embedding{positions:coords.iter().map(|p|DVec::new(*p)).collect(),graph:&GRAPH};
 let engine:Option<Box<dyn Query<2>>>=if n==0{None}else{Some(match kind {
 0=>Box::new(Spark(sprk::Sprk::new(&coords))),
 1=>Box::new(kiddo::Kiddo::new(e)),
 2=>Box::new(nabo::Nabo::new(e)),
 3=>Box::new(neighbourhood::Neihbourhood::new(e)),
 4=>Box::new(vptree::VPTree::new(e)),
 5=>Box::new(orthtree::Orthtree::new(e)),
 6=>Box::new(grid::Grid::new_with_radius(e,radius_hint)),
 7|8|9=>Box::new(PyIndex::new(&coords,kind)),
 _=>panic!("unknown ESA backend"),
 })};
 Box::into_raw(Box::new(Index{points,engine,coordinate_engine:None,hits:Vec::new(),output:Vec::new()})).cast()
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn esa_destroy(p:*mut std::ffi::c_void){drop(unsafe{Box::from_raw(p.cast::<Index>())});}
#[unsafe(no_mangle)] pub unsafe extern "C" fn esa_query(p:*mut std::ffi::c_void,x0:u32,y0:u32,x1:u32,y1:u32,out:*mut *const u32)->usize{
 let index=unsafe{&mut *p.cast::<Index>()}; index.hits.clear();index.output.clear();
 if let Some(tree)=&index.coordinate_engine {
  let center=[(x0 as f64+x1 as f64)/8589934592.0,(y0 as f64+y1 as f64)/8589934592.0];
  let dx=(x1 as f64-x0 as f64)/8589934592.0;let dy=(y1 as f64-y0 as f64)/8589934592.0;
  for p in tree.neighbourhood(&center,(dx*dx+dy*dy+64.0*f64::EPSILON).sqrt()) {
   let p=p.map(|v|(v*4294967296.0) as u32);if p[0]>=x0&&p[0]<=x1&&p[1]>=y0&&p[1]<=y1{index.output.push(p);}
  }
 }
 if x0<=x1 && y0<=y1 {if let Some(engine)=&index.engine {
 let center=DVec::new([(x0 as f64+x1 as f64)/8589934592.0,(y0 as f64+y1 as f64)/8589934592.0]);
 let dx=(x1 as f64-x0 as f64)/8589934592.0; let dy=(y1 as f64-y0 as f64)/8589934592.0;
 // Conservative absolute margin covers SNN's cancellation in squared distances.
 let r=(dx*dx+dy*dy+64.0*f64::EPSILON).sqrt();
 engine.query_radius(center,r,&mut index.hits);
 for &id in &index.hits {let q=index.points[id];if q[0]>=x0&&q[0]<=x1&&q[1]>=y0&&q[1]<=y1{index.output.push(q);}}
 }}
 unsafe{*out=index.output.as_ptr().cast()};index.output.len()
}


mod dynamic_kiddo;

// Internal bridge only: the C++ adapter enumerates these IDs into explicit
// integer point output and applies the exact circle predicate once.
// Borrowed buffers remain valid until the next query or destruction.
#[unsafe(no_mangle)] pub unsafe extern "C" fn esa_circle_candidates(p:*mut std::ffi::c_void,x:u32,y:u32,r:f64,ids:*mut *const usize,points:*mut *const u32)->usize {
 let index=unsafe{&mut *p.cast::<Index>()};index.hits.clear();
 if let Some(engine)=&index.engine {
  engine.query_radius(DVec::new([x as f64/4294967296.0,y as f64/4294967296.0]),r,&mut index.hits);
 }
 unsafe{*ids=index.hits.as_ptr();*points=index.points.as_ptr().cast()};index.hits.len()
}

// Circle campaign ABI: unchanged upstream engines, original integer output.
#[unsafe(no_mangle)] pub unsafe extern "C" fn esa_circle_query(p:*mut std::ffi::c_void,x:u32,y:u32,r:f64,out:*mut *const u32)->usize {
 let index=unsafe{&mut *p.cast::<Index>()};index.hits.clear();index.output.clear();
 if let Some(tree)=&index.coordinate_engine {
  for p in tree.neighbourhood(&[x as f64/4294967296.0,y as f64/4294967296.0],r){index.output.push(p.map(|v|(v*4294967296.0) as u32));}
 }
 if let Some(engine)=&index.engine {
  engine.query_radius(DVec::new([x as f64/4294967296.0,y as f64/4294967296.0]),r,&mut index.hits);
  for &id in &index.hits { index.output.push(index.points[id]); }
 }
 unsafe{*out=index.output.as_ptr().cast()};index.output.len()
}

// Runtime initialization is outside index construction for every bridge entry.
#[unsafe(no_mangle)] pub extern "C" fn esa_prepare_runtime(kind:u32) {
 if kind>=7 && kind<=9 { Python::attach(|py| {
  py.import("numpy").unwrap();
  py.import(if kind==9 {"snnpy"} else {"sklearn.neighbors"}).unwrap();
 }); }
}
