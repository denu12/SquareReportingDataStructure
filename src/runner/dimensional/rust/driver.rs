use std::io::Read;
fn u32_read(f:&mut impl Read)->u32{let mut b=[0;4];f.read_exact(&mut b).unwrap();u32::from_le_bytes(b)}
fn u64_read(f:&mut impl Read)->u64{let mut b=[0;8];f.read_exact(&mut b).unwrap();u64::from_le_bytes(b)}
fn contains<const D:usize>(p:&[u32;D],c:&[u32;D],r2:u128)->bool {
 p.iter().zip(c).map(|(&x,&y)|{let v=(x as i64-y as i64).unsigned_abs() as u128;v*v}).sum::<u128>()<=r2
}
fn run<const D:usize>(f:&mut impl Read,kind:&str){
 let n=u64_read(f) as usize;let q=u64_read(f) as usize;
 let points:Vec<[u32;D]>=(0..n).map(|_|std::array::from_fn(|_|u32_read(f))).collect();
 let queries:Vec<([u32;D],u128)>=(0..q).map(|_|{
  let center=std::array::from_fn(|_|u32_read(f));let low=u64_read(f) as u128;
  (center,low|((u64_read(f) as u128)<<64))
 }).collect();
 let bench=std::env::args().any(|x|x=="--bench");
 let kind=if IDS_ONLY {"esa_sprk"} else {kind};
 let start=std::time::Instant::now();
 let radius_hint=if q==0 {0.001} else {queries.iter().map(|(_,r)|(*r as f64).sqrt()/4294967296.0).sum::<f64>()/q as f64};
 let tree=if n==0{None}else{Some(engine(&points,kind,radius_hint))};
 let build_seconds=start.elapsed().as_secs_f64();let mut query_seconds=0.0;let mut answers=0;
 let mut integer_mismatch_queries=0;
 let mut ids=Vec::new();let mut actual=Vec::new();let batch_start=std::time::Instant::now();
 for (j,(center,r2)) in queries.iter().enumerate(){
  let start=if bench {None} else {Some(std::time::Instant::now())};
  ids.clear();actual.clear();
  if let Some(t)=&tree{t.query_radius(query_point(*center),search_radius::<D>(*r2,kind),&mut ids);}
  if IDS_ONLY {
   // SPRK (original): native floating matches, no timed integer filter or coordinates.
   std::hint::black_box(&ids);answers+=ids.len();
  } else {
   actual.extend(ids.iter().filter_map(|&id|{let p=points[id];contains(&p,center,*r2).then_some(p)}));
   std::hint::black_box(&actual);answers+=actual.len();
  }
  if let Some(start)=start{query_seconds+=start.elapsed().as_secs_f64();}
  if bench{continue;}
  if IDS_ONLY{
   if ids.iter().any(|&id|id>=n){println!("{{\"status\":\"incorrect\",\"reason\":\"invalid_id\"}}");std::process::exit(2);}
   actual.extend(ids.iter().map(|&id|points[id]));
  }
  let mut expected:Vec<_>=points.iter().filter(|p|contains(p,center,*r2)).copied().collect();
  expected.sort_unstable();actual.sort_unstable();
  if IDS_ONLY {
   if actual!=expected {integer_mismatch_queries+=1;}
   // Diagnostic admission uses its disclosed expanded f64 ball.
   let radius=search_radius::<D>(*r2,kind);
   expected=points.iter().filter(|p| {
    let dx=(p[0] as f64-center[0] as f64)/4294967296.0;
    let mut squared=dx*dx;
    for axis in 1..D {let dx=(p[axis] as f64-center[axis] as f64)/4294967296.0;squared=dx.mul_add(dx,squared);}
    squared<=radius*radius
   }).copied().collect();expected.sort_unstable();
  }
  if actual!=expected{println!("{{\"status\":\"incorrect\",\"query\":{},\"actual_count\":{},\"expected_count\":{}}}",j,actual.len(),expected.len());std::process::exit(2);}
 }
 if bench{query_seconds=batch_start.elapsed().as_secs_f64();}
 println!("{{\"status\":\"success\",\"queries\":{},\"answers\":{},\"build_seconds\":{},\"query_seconds\":{},\"output_contract\":\"{}\",\"membership_contract\":\"{}\",\"integer_mismatch_queries\":{}}}",q,answers,build_seconds,query_seconds,if IDS_ONLY{"point_ids_only"}else{"integer_points"},if IDS_ONLY{"expanded_f64_ball"}else{"closed_integer_ball"},integer_mismatch_queries);
}
fn main(){let args:Vec<String>=std::env::args().collect();let mut f=std::fs::File::open(&args[1]).unwrap();let d=u32_read(&mut f);
 match d{2=>run::<2>(&mut f,&args[2]),3=>run::<3>(&mut f,&args[2]),4=>run::<4>(&mut f,&args[2]),5=>run::<5>(&mut f,&args[2]),6=>run::<6>(&mut f,&args[2]),7=>run::<7>(&mut f,&args[2]),8=>run::<8>(&mut f,&args[2]),9=>run::<9>(&mut f,&args[2]),10=>run::<10>(&mut f,&args[2]),11=>run::<11>(&mut f,&args[2]),12=>run::<12>(&mut f,&args[2]),13=>run::<13>(&mut f,&args[2]),14=>run::<14>(&mut f,&args[2]),15=>run::<15>(&mut f,&args[2]),16=>run::<16>(&mut f,&args[2]),32=>run::<32>(&mut f,&args[2]),_=>panic!("Unsupported dimension")}
}
