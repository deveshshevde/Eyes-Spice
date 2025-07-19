
/* Result */
/* 
[Benchmark Result]
  Total transferred: 3.05 MB
  Elapsed time     : 5.50 s
  Bandwidth        : 0.55 MB/s
[Server] Connection from 192.168.1.33:59573
[1] Received and echoed 160000 bytes
[2] Received and echoed 160000 bytes
[3] Received and echoed 160000 bytes
[4] Received and echoed 160000 bytes
[5] Received and echoed 160000 bytes
[6] Received and echoed 160000 bytes
[7] Received and echoed 160000 bytes
[8] Received and echoed 160000 bytes
[9] Received and echoed 160000 bytes
[10] Received and echoed 160000 bytes
*/

use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Instant;

const SAMPLE_RATE: usize = 16_000;
const DURATION_SECS: usize = 5;
const CHANNELS: usize = 1;  // mono sound
const BYTES_PER_SAMPLE: usize = 2; 

const BUFFER_SIZE: usize = SAMPLE_RATE * DURATION_SECS * CHANNELS * BYTES_PER_SAMPLE;
const ITERATIONS: usize = 10;

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8000").await?;
    println!("[Server] Listening on port 8000...");

    loop {
        let (mut socket, addr) = listener.accept().await?;
        println!("[Server] Connection from {}", addr);

        tokio::spawn(async move {
            let mut total_bytes: usize = 0;
            let mut buf = vec![0u8; BUFFER_SIZE];

            let start = Instant::now();

            for i in 0..ITERATIONS {
                let mut received = 0;
                while received < BUFFER_SIZE {
                    match socket.read(&mut buf[received..]).await {
                        Ok(0) => {
                            println!("[Server] Connection closed");
                            return;
                        }
                        Ok(n) => received += n,
                        Err(e) => {
                            eprintln!("[Server] Read error: {}", e);
                            return;
                        }
                    }
                }

                total_bytes += received;

                if let Err(e) = socket.write_all(&buf).await {
                    eprintln!("[Server] Write error: {}", e);
                    return;
                }

                println!("[{}] Received and echoed {} bytes", i + 1, received);
            }

            let elapsed = start.elapsed().as_secs_f64();
            let mb = (total_bytes * 2) as f64 / (1024.0 * 1024.0); 
            println!("\n[Benchmark Result]");
            println!("  Total transferred: {:.2} MB", mb);
            println!("  Elapsed time     : {:.2} s", elapsed);
            println!("  Bandwidth        : {:.2} MB/s", mb / elapsed);
        });
    }
}
