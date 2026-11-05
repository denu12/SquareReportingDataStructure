use std::{fs::File,io::{Read,Seek,SeekFrom,BufReader},hint::black_box,time::Instant};
// use sprk::integer_reporting::IntegerPointSprk; and ALGORITHM are replaced when the two independent runners are generated.
use sprk::integer_reporting::IntegerPointSprk;
#[cfg(not(feature = "compiler-only"))]
const ALGORITHM:&str="esa_sprk_RUST_EXACT";
#[cfg(feature = "compiler-only")]
const ALGORITHM:&str="esa_sprk_RUST_EXACT_NOSIMD";
fn read<const N:usize>(f:&mut impl Read)->std::io::Result<[u8;N]>{let mut b=[0;N];f.read_exact(&mut b)?;Ok(b)}
fn u32r(f:&mut impl Read)->std::io::Result<u32>{Ok(u32::from_le_bytes(read(f)?))}
fn u64r(f:&mut impl Read)->std::io::Result<u64>{Ok(u64::from_le_bytes(read(f)?))}
fn point(f:&mut impl Read)->std::io::Result<[u32;2]>{Ok([u32r(f)?,u32r(f)?])}
fn contains(p:[u32;2],c:[u32;2],r:u128)->bool{p.iter().zip(c).map(|(&x,y)|{let d=(x as i64-y as i64).unsigned_abs() as u128;d*d}).sum::<u128>()<=r}
fn main(){if let Err(e)=run(){eprintln!("{e}");std::process::exit(2)}}
fn run()->Result<(),Box<dyn std::error::Error>>{
 let args:Vec<_>=std::env::args().collect();
 let (path,bench,dest)=if args.len()==5&&args[1]==ALGORITHM {if !["bench","verify"].contains(&args[3].as_str()){return Err("Bad mode".into())}(&args[2],args[3]=="bench",Some(&args[4]))}
 else if (args.len()==3||args.len()==4)&&args[2]==ALGORITHM{if args.len()==4&&args[3]!="--bench"{return Err("Bad mode".into())}(&args[1],args.len()==4,None)}else{return Err("Bad arguments".into())};
 let mut f=BufReader::new(File::open(path)?);let magic=read::<8>(&mut f)?;if &magic!=b"ESA2D01\0"{f.seek(SeekFrom::Start(0))?;if u32r(&mut f)?!=2{return Err("Unsupported dimension".into())}}
 let n=u64r(&mut f)?;let q=u64r(&mut f)?;if n>1000000000||q>1000000000{return Err("Bad counts".into())}
 let p:Vec<_>=(0..n).map(|_|point(&mut f)).collect::<Result<_,_>>()?;
 let qs:Vec<_>=(0..q).map(|_|{let c=point(&mut f)?;let lo=u64r(&mut f)?;let hi=u64r(&mut f)?;Ok((c,lo as u128|(hi as u128)<<64))}).collect::<std::io::Result<_>>()?;
 let mut tail=[0;1];if f.read(&mut tail)?!=0{return Err("Trailing input data".into())}
 let start=Instant::now();let mut tree=IntegerPointSprk::new(&p);
 let build=start.elapsed().as_secs_f64();let mut out=Vec::new();let mut answers=0u64;let mut completed=0;let mut failed=None;let mut elapsed=0.0;let batch=Instant::now();
 for (j,&(c,r)) in qs.iter().enumerate(){let start=(!bench).then(Instant::now);out.clear();tree.query_points(c,r,&mut out);
  black_box(&out);answers+=out.len() as u64;completed+=1;if let Some(t)=start{elapsed+=t.elapsed().as_secs_f64()}
  if !bench{let mut expected:Vec<_>=p.iter().copied().filter(|&p|contains(p,c,r)).collect();out.sort_unstable();expected.sort_unstable();if out!=expected{failed=Some(j);break}}
 }
 if bench{elapsed=batch.elapsed().as_secs_f64()}
 let status=if failed.is_none(){"success"}else{"incorrect"};let fail=failed.map(|x|x.to_string()).unwrap_or("null".into());
 let result=format!("{{\"status\":\"{status}\",\"algorithm\":\"{ALGORITHM}\",\"dimension\":2,\"queries\":{completed},\"answers\":{answers},\"build_seconds\":{build},\"query_seconds\":{elapsed},\"update_seconds\":0,\"insertions\":0,\"deletions\":0,\"output_contract\":\"integer_points\",\"membership_contract\":\"closed_integer_ball\",\"failed_query\":{fail}}}");
 if let Some(path)=dest{std::fs::write(path,format!("{result}\n"))?}println!("{result}");if failed.is_some(){std::process::exit(2)}Ok(())
}
