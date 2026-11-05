//! Local 3D extension of the ESA authors' exact IntegerPointSprk adaptation.
//! Keeps their conservative f32 leaf-range search, with exact original u32 triples.
//! Three squared coordinate differences need 66 bits; carry counts are retained.
use crate::integer_reporting::candidate_radius;
use crate::scalar::IdStorage;
use crate::tree::{LeafRange, Sprk};

#[derive(Clone)]
#[repr(C, align(64))]
struct Block {
    axes: [[u32; 8]; 3],
    ids: [u32; 8],
    valid: u8,
}
/// Exact 3D coordinate reporting with a reusable eight-point leaf payload.
pub struct IntegerPointSprk3D {
    tree: Option<Sprk<3, 8, f32, u32>>,
    blocks: Vec<Block>,
    ranges: Vec<LeafRange>,
}
fn distance(p: [u32; 3], c: [u32; 3]) -> u128 {
    p.into_iter().zip(c).map(|(x,y)| { let d=x.abs_diff(y) as u128; d*d }).sum()
}
fn mask(block: &Block, center: [u32; 3], radius2: u128) -> u8 {
    #[cfg(all(not(feature="compiler-only"), target_arch="x86_64", target_feature="avx512f"))]
    if radius2 <= 3 * u32::MAX as u128 * u32::MAX as u128 { unsafe {
        use std::arch::x86_64::*;
        let mut sum=_mm512_setzero_si512();let mut high=sum;
        let one=_mm512_set1_epi64(1);
        for a in 0..3 {
            let p=_mm512_cvtepu32_epi64(_mm256_loadu_si256(block.axes[a].as_ptr().cast()));
            let c=_mm512_set1_epi64(center[a] as i64);
            let d=_mm512_sub_epi64(_mm512_max_epu64(p,c),_mm512_min_epu64(p,c));
            let next=_mm512_add_epi64(sum,_mm512_mul_epu32(d,d));
            let carry=_mm512_cmplt_epu64_mask(next,sum);
            high=_mm512_mask_add_epi64(high,carry,high,one);sum=next;
        }
        let hi=_mm512_set1_epi64((radius2>>64) as i64);
        let lo=_mm512_set1_epi64(radius2 as u64 as i64);
        return block.valid & (_mm512_cmplt_epu64_mask(high,hi)
            | (_mm512_cmpeq_epi64_mask(high,hi) & _mm512_cmple_epu64_mask(sum,lo)));
    }}
    #[cfg(all(not(feature="compiler-only"), target_arch="x86_64", target_feature="avx2", not(target_feature="avx512f")))]
    if radius2 <= 3 * u32::MAX as u128 * u32::MAX as u128 { unsafe {
        use std::arch::x86_64::*;
        #[inline] unsafe fn less(a: __m256i,b: __m256i)->__m256i { unsafe {
            let sign=_mm256_set1_epi64x(i64::MIN);
            _mm256_cmpgt_epi64(_mm256_xor_si256(b,sign),_mm256_xor_si256(a,sign))
        }}
        let mut result=0;
        for first in [0,4] {
            let mut sum=_mm256_setzero_si256();let mut high=sum;
            for a in 0..3 {
                let p=_mm_loadu_si128(block.axes[a].as_ptr().add(first).cast());
                let c=_mm_set1_epi32(center[a] as i32);
                let d=_mm256_cvtepu32_epi64(_mm_sub_epi32(_mm_max_epu32(p,c),_mm_min_epu32(p,c)));
                let next=_mm256_add_epi64(sum,_mm256_mul_epu32(d,d));
                high=_mm256_add_epi64(high,_mm256_and_si256(less(next,sum),_mm256_set1_epi64x(1)));sum=next;
            }
            let hi=_mm256_set1_epi64x((radius2>>64) as i64);
            let lo=_mm256_set1_epi64x(radius2 as u64 as i64);
            let accepted=_mm256_or_si256(less(high,hi),_mm256_andnot_si256(less(lo,sum),_mm256_cmpeq_epi64(high,hi)));
            result|=(_mm256_movemask_pd(_mm256_castsi256_pd(accepted)) as u8)<<first;
        }
        return result & block.valid;
    }}
    let mut result=0;
    for i in 0..8 {
        let p=std::array::from_fn(|a|block.axes[a][i]);
        if distance(p,center)<=radius2 {result|=1<<i;}
    }
    result & block.valid
}
impl IntegerPointSprk3D {
    /// Builds the f32 traversal tree and retains original coordinates in leaf order.
    pub fn new(points: &[[u32; 3]]) -> Self {
        assert!(points.len() < u32::MAX as usize, "SPRK3D requires fewer than u32::MAX points");
        if points.is_empty() {return Self {tree:None,blocks:Vec::new(),ranges:Vec::new()};}
        let coords: Vec<_>=points.iter().map(|p|p.map(|x|(x as f64/4294967296.0) as f32)).collect();
        let tree=Sprk::new(&coords);
        let blocks=tree.positions_sorted.iter().map(|v| {
            let mut b=Block {axes:[[0;8];3],ids:v.ids,valid:0};
            for (i,&id) in v.ids.iter().enumerate() {
                if id==u32::SENTINEL {continue;}
                for a in 0..3 {b.axes[a][i]=points[id as usize][a];}
                b.valid|=1<<i;
            }b
        }).collect();
        Self {tree:Some(tree),blocks,ranges:Vec::new()}
    }
    fn scan(&mut self, center: [u32; 3], radius2: u128, mut emit: impl FnMut(&Block,u8)) {
        let Some(tree)=&self.tree else {return};
        if radius2>=3*u32::MAX as u128*u32::MAX as u128 {
            for b in &self.blocks {emit(b,b.valid);}return;
        }
        let p=center.map(|x|(x as f64/4294967296.0) as f32);
        let r=candidate_radius::<3>((radius2 as f64).sqrt()/4294967296.0);
        self.ranges.clear();tree.collect_ranges(&p,0,0,r*r,&mut [0.0;3],&mut self.ranges);
        for range in &self.ranges {
            for b in &self.blocks[range.min_i..range.max_i] {emit(b,mask(b,center,radius2));}
        }
    }
    /// Appends exact original triples, preserving duplicates and existing output.
    pub fn query_points(&mut self, center: [u32; 3], radius2: u128, out: &mut Vec<[u32; 3]>) {
        self.scan(center,radius2,|b,mut selected| {
            out.reserve(selected.count_ones() as usize);
            while selected!=0 {
                let i=selected.trailing_zeros() as usize;selected&=selected-1;
                // Each selected original triple is materialized during the query.
                out.push(std::array::from_fn(|a|b.axes[a][i]));
            }
        });
    }
    /// Appends original point indices for multiset and duplicate validation.
    pub fn query_ids(&mut self, center: [u32; 3], radius2: u128, out: &mut Vec<usize>) {
        self.scan(center,radius2,|b,mut selected| {
            out.reserve(selected.count_ones() as usize);
            while selected!=0 {let i=selected.trailing_zeros() as usize;selected&=selected-1;out.push(b.ids[i] as usize);}
        });
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn random(s: &mut u64)->u32 {*s^=*s<<13;*s^=*s>>7;*s^=*s<<17;(*s>>32) as u32}
    #[test]
    fn integer_mask_retains_both_carries() {
        let mut b=Block {axes:[[0;8];3],ids:[0;8],valid:255};
        for i in 0..8 {for a in 0..3 {b.axes[a][i]=if i>>a&1==1 {u32::MAX} else {0};}}
        for c in [[0;3],[u32::MAX;3],[u32::MAX,0,u32::MAX],[1<<31;3]] {
            for i in 0..8 {
                let d=distance(std::array::from_fn(|a|b.axes[a][i]),c);
                for r in [d.saturating_sub(1),d,d+1,1u128<<64,1u128<<65,u128::MAX] {
                    let expected=(0..8).fold(0,|m,j|m | (u8::from(distance(std::array::from_fn(|a|b.axes[a][j]),c)<=r)<<j));
                    assert_eq!(mask(&b,c,r),expected);
                }
            }
        }
    }
    #[test]
    fn exact_points_ids_duplicates_tails_and_boundaries() {
        let mut seed=123456789;
        for n in [0,1,3,4,7,8,9,15,16,17,63,64,65,127,128,129,513,1025] {
            for mode in 0..4 {
                let p: Vec<[u32;3]>=(0..n).map(|_|std::array::from_fn(|_|{
                    let v=random(&mut seed);match mode {1=>u32::MAX-v%32,2=>77,3=>v&0xff000000,_=>v}
                })).collect();
                let mut tree=IntegerPointSprk3D::new(&p);
                for q in 0..20 {
                    let c=match q {0=>[0;3],1=>[u32::MAX;3],2 if n>0=>p[n-1],_=>std::array::from_fn(|_|random(&mut seed))};
                    let d=if n==0 {0} else {distance(p[q%n],c)};
                    for r in [0,1,(1u128<<64)-1,1u128<<64,(1u128<<64)+1,(1u128<<65)-1,1u128<<65,(1u128<<65)+1,3*u32::MAX as u128*u32::MAX as u128,u128::MAX,d.saturating_sub(1),d,d+1] {
                        let mut expected: Vec<_>=p.iter().copied().filter(|&v|distance(v,c)<=r).collect();
                        let mut got=vec![[17,18,19]];tree.query_points(c,r,&mut got);assert_eq!(got.remove(0),[17,18,19]);
                        expected.sort_unstable();got.sort_unstable();assert_eq!(got,expected,"n={n}, mode={mode}, q={q}, r={r}");
                        let mut ids=vec![usize::MAX];tree.query_ids(c,r,&mut ids);assert_eq!(ids.remove(0),usize::MAX);ids.sort_unstable();
                        let expected_ids: Vec<_>=p.iter().enumerate().filter_map(|(i,&v)|(distance(v,c)<=r).then_some(i)).collect();assert_eq!(ids,expected_ids);
                    }
                }
            }
        }
    }
}
