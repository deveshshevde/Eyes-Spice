/*
[Benchmark Result]
  Total transferred: 6.26 MB
  Elapsed time     : 33.04 s
  Bandwidth        : 0.19 MB/s
[42] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.41 MB
  Elapsed time     : 33.85 s
  Bandwidth        : 0.19 MB/s
[43] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.56 MB
  Elapsed time     : 34.63 s
  Bandwidth        : 0.19 MB/s
[44] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.71 MB
  Elapsed time     : 35.54 s
  Bandwidth        : 0.19 MB/s
[45] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.87 MB
  Elapsed time     : 36.30 s
  Bandwidth        : 0.19 MB/s

*/
use actix_web::{post, web, App, HttpServer, HttpResponse, Responder};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Instant;
use std::sync::Mutex;
use once_cell::sync::Lazy;

const EXPECTED_ITERATIONS: usize = 10;

static COUNTER: AtomicUsize = AtomicUsize::new(0);
static TOTAL_BYTES: AtomicUsize = AtomicUsize::new(0);
static START_TIME: Lazy<Mutex<Option<Instant>>> = Lazy::new(|| Mutex::new(None));

#[post("/upload")]
async fn upload(body: web::Bytes) -> impl Responder {
    let count = COUNTER.fetch_add(1, Ordering::SeqCst) + 1;
    let size = body.len();
    TOTAL_BYTES.fetch_add(size, Ordering::SeqCst);

    let mut start = START_TIME.lock().unwrap();
    if start.is_none() {
        *start = Some(Instant::now());
        println!("[Server] First connection received.");
    }

    println!("[{}] Received {} bytes", count, size);

    if count >= EXPECTED_ITERATIONS {
        let elapsed = start.unwrap().elapsed().as_secs_f64();
        let bytes = TOTAL_BYTES.load(Ordering::SeqCst);
        let mb = bytes as f64 / (1024.0 * 1024.0);
        let mbps = mb / elapsed;

        println!("\n[Benchmark Result]");
        println!("  Total transferred: {:.2} MB", mb);
        println!("  Elapsed time     : {:.2} s", elapsed);
        println!("  Bandwidth        : {:.2} MB/s", mbps);
    }

    HttpResponse::Ok().body("OK")
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    println!("🚀 Listening on http://0.0.0.0:8080");

    HttpServer::new(|| App::new().service(upload))
        .bind(("0.0.0.0", 8080))?
        .run()
        .await
}
