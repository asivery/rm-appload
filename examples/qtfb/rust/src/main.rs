use qtfb_client::ClientConnection;

fn main() {
    let client = ClientConnection::new(
        qtfb_client::constants::DEFAULT_SCENE,
        qtfb_client::constants::FBFMT_RMPP_RGB888,
        None
    ).unwrap();
    let file_contents = std::fs::read("a.raw").unwrap();
    client.shm[0..file_contents.len()].copy_from_slice(&file_contents);
    client.send_complete_update().unwrap();
    std::thread::sleep(std::time::Duration::from_secs(10));
}
