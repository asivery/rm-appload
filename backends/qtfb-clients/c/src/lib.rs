mod capi {
    use std::ffi::c_void;
    use libc::c_int;
    use qtfb_client;

    pub const DEFAULT_SCENE: u32 = 245209899;
    #[no_mangle]
    pub static SOCKET_PATH: &[u8] = b"/tmp/qtfb.sock\0";
    pub const MESSAGE_INITIALIZE: u8 = 0;
    pub const MESSAGE_UPDATE: u8 = 1;
    pub const MESSAGE_CUSTOM_INITIALIZE: u8 = 2;
    pub const UPDATE_ALL: i32 = 0;
    pub const UPDATE_PARTIAL: i32 = 1;
    pub const FBFMT_RM2FB: u8 = 0;
    pub const FBFMT_RMPP_RGB888: u8 = 1;
    pub const FBFMT_RMPP_RGBA8888: u8 = 2;

    pub type FBKey = u32;

    pub type ClientConnection = c_void;


    #[repr(C)]
    struct CustomResolution {
        width: u16,
        height: u16,
    }

    #[no_mangle]
    unsafe extern "C" fn create_connection<'a>(framebuffer_id: FBKey,
                                               shm_type: u8,
                                               custom_resolution: *const CustomResolution,
    ) -> *mut qtfb_client::ClientConnection<'a> {
        //TODO: check with and height ordered correctly
        Box::into_raw(Box::new(qtfb_client::ClientConnection::new(framebuffer_id, shm_type, custom_resolution.as_ref().and_then(|t| Some((t.height, t.width)))).unwrap()))
    }
    #[no_mangle]
    unsafe extern "C" fn send_complete_update(connection: *mut qtfb_client::ClientConnection) -> c_int {
        connection.as_ref().unwrap().send_complete_update().map_or_else(|e| e.raw_os_error().unwrap_or(-1), |_| 0)
    }
    #[no_mangle]
    unsafe extern "C" fn send_partial_update(connection: *mut qtfb_client::ClientConnection, x: i32, y: i32, w: i32, h: i32) -> c_int {
        connection.as_ref().unwrap().send_partial_update(x, y, w, h).map_or_else(|e| e.raw_os_error().unwrap_or(-1), |_| 0)
    }
    #[no_mangle]
    unsafe extern "C" fn destroy_connection(connection: *mut qtfb_client::ClientConnection) {
        if connection.is_null() {
            return;
        }
        let _ = Box::from_raw(connection);
    }
    #[no_mangle]
    extern "C" fn get_socket_path() -> *const libc::c_char {
        SOCKET_PATH.as_ptr() as *const libc::c_char
    }

    #[no_mangle]
    unsafe extern "C" fn get_buffer(connection: *mut qtfb_client::ClientConnection) -> *const u8 {
        return connection.as_ref().unwrap().shm.as_ptr()
    }
}