use kiddo::{KdTree,SquaredEuclidean};
struct Index {tree:KdTree<f64,2>,out:Vec<[u32;2]>}
fn coords(x:u32,y:u32)->[f64;2]{[x as f64/4294967296.0,y as f64/4294967296.0]}
fn key(x:u32,y:u32)->u64{((x as u64)<<32)|y as u64}
#[unsafe(no_mangle)]pub unsafe extern "C" fn kiddo_dyn_circle_query(p:*mut std::ffi::c_void,x:u32,y:u32,radius2:f64,out:*mut *const u32)->usize{
 let i=unsafe{&mut *p.cast::<Index>()};i.out.clear();
 for h in i.tree.within_unsorted_iter::<SquaredEuclidean>(&coords(x,y),radius2){i.out.push([(h.item>>32) as u32,h.item as u32]);}
 unsafe{*out=i.out.as_ptr().cast()};i.out.len()
}
#[unsafe(no_mangle)]pub extern "C" fn kiddo_dyn_create()->*mut std::ffi::c_void{Box::into_raw(Box::new(Index{tree:KdTree::new(),out:Vec::new()})).cast()}
#[unsafe(no_mangle)]pub unsafe extern "C" fn kiddo_dyn_destroy(p:*mut std::ffi::c_void){drop(unsafe{Box::from_raw(p.cast::<Index>())});}
#[unsafe(no_mangle)]pub unsafe extern "C" fn kiddo_dyn_insert(p:*mut std::ffi::c_void,x:u32,y:u32){unsafe{&mut *p.cast::<Index>()}.tree.add(&coords(x,y),key(x,y));}
#[unsafe(no_mangle)]pub unsafe extern "C" fn kiddo_dyn_erase(p:*mut std::ffi::c_void,x:u32,y:u32){let n=unsafe{&mut *p.cast::<Index>()}.tree.remove(&coords(x,y),key(x,y));assert_eq!(n,1);}
#[unsafe(no_mangle)]pub unsafe extern "C" fn kiddo_dyn_query(p:*mut std::ffi::c_void,x0:u32,y0:u32,x1:u32,y1:u32,out:*mut *const u32)->usize{
 let i=unsafe{&mut *p.cast::<Index>()};i.out.clear();
 let c=[(x0 as f64+x1 as f64)/8589934592.0,(y0 as f64+y1 as f64)/8589934592.0];
 let dx=(x1 as f64-x0 as f64)/8589934592.0;let dy=(y1 as f64-y0 as f64)/8589934592.0;
 for h in i.tree.within_unsorted::<SquaredEuclidean>(&c,dx*dx+dy*dy+64.0*f64::EPSILON){let x=(h.item>>32) as u32;let y=h.item as u32;if x>=x0&&x<=x1&&y>=y0&&y<=y1{i.out.push([x,y]);}}
 unsafe{*out=i.out.as_ptr().cast()};i.out.len()
}
