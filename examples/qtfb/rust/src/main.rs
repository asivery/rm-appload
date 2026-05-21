use qtfb_client::ClientConnection;
use std::env;

fn main() {
    let key: u32 =  env::var("QTFB_KEY").expect("couldnt find QTFB_KEY environment variable. Are you sure you launched from appload?").parse::<u32>().expect("qtfb not a number");
    let client = ClientConnection::new(
        key,
        qtfb_client::constants::FBFMT_RMPP_RGB888,
        None
    ).unwrap();
    println!("hii1");
    let file_contents = std::fs::read("a.raw").unwrap();
    client.shm[0..file_contents.len()].copy_from_slice(&file_contents);
    let res = client.send_complete_update().unwrap();
    println!("hii2 {:?}", res);
    std::thread::sleep(std::time::Duration::from_secs(10));
}
