use crate::{NodeId, dvec::DVec};
pub trait Graph { fn is_connected(&self,a:NodeId,b:NodeId)->bool; fn neighbors(&self,i:NodeId)->&[NodeId]; fn weight(&self,i:NodeId)->f64; }
pub trait Position<const D:usize> { fn position(&self,i:NodeId)->&DVec<D>; fn num_nodes(&self)->usize; }
pub trait Update<const D:usize> {fn update_positions(&mut self,p:&[DVec<D>],d:Option<f64>);}
pub trait Query<const D:usize> { fn query_radius(&self,p:DVec<D>,r:f64,out:&mut Vec<NodeId>); }
pub trait SpatialIndex<const D:usize>: Query<D> {fn name(&self)->String; fn implementation_string(&self)->&'static str; fn set_radius_hint(&mut self,_r:f64){} }
pub trait Embedder<'a,const D:usize> {fn new(e:&crate::Embedding<'a,D>)->Self;}
