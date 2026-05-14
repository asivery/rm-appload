mod dumbstupidhack {
    pub(crate) use qtfb_client::ClientConnection as QTFB_ClientConnection;
    pub(crate) use qtfb_client::InputMessage as QTFB_InputMessage;
}
mod capi {
    use std::ffi::c_void;
    use std::ptr::null;
    use libc::c_int;
    use qtfb_client;
    use qtfb_client::user_input::InputEvent;
    use crate::dumbstupidhack;


    pub const QTFB_DEFAULT_SCENE: u32 = 245209899;
    #[no_mangle]
    pub static QTFB_SOCKET_PATH: &[u8] = b"/tmp/qtfb.sock\0";
    pub const QTFB_MESSAGE_INITIALIZE: u8 = 0;
    pub const QTFB_MESSAGE_UPDATE: u8 = 1;
    pub const QTFB_MESSAGE_CUSTOM_INITIALIZE: u8 = 2;
    pub const QTFB_UPDATE_ALL: i32 = 0;
    pub const QTFB_UPDATE_PARTIAL: i32 = 1;
    pub const QTFB_FBFMT_RM2FB: u8 = 0;
    pub const QTFB_FBFMT_RMPP_RGB888: u8 = 1;
    pub const QTFB_FBFMT_RMPP_RGBA8888: u8 = 2;

    pub type QTFB_FBKey = u32;

    pub type QTFB_ClientConnection = c_void;

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct QTFB_InputMessage {
        input_type: u32,
        dev_id: u32,
        x: u32, y: u32, d: u32
    }


    #[repr(C)]
    struct QTFB_CustomResolution {
        width: u16,
        height: u16,
    }

    #[no_mangle]
    unsafe extern "C" fn qtfb_create_connection<'a>(framebuffer_id: QTFB_FBKey,
                                               shm_type: u8,
                                               custom_resolution: *const QTFB_CustomResolution,
    ) -> *mut dumbstupidhack::QTFB_ClientConnection<'a> {
        Box::into_raw(Box::new(dumbstupidhack::QTFB_ClientConnection::new(framebuffer_id, shm_type, custom_resolution.as_ref().and_then(|t| Some((t.width, t.height)))).unwrap()))
    }
    #[no_mangle]
    unsafe extern "C" fn qtfb_send_complete_update(connection: *mut dumbstupidhack::QTFB_ClientConnection) -> c_int {
        connection.as_ref().unwrap().send_complete_update().map_or_else(|e| e.raw_os_error().unwrap_or(-1), |_| 0)
    }
    #[no_mangle]
    unsafe extern "C" fn qtfb_send_partial_update(connection: *mut dumbstupidhack::QTFB_ClientConnection, x: i32, y: i32, w: i32, h: i32) -> c_int {
        connection.as_ref().unwrap().send_partial_update(x, y, w, h).map_or_else(|e| e.raw_os_error().unwrap_or(-1), |_| 0)
    }
    #[no_mangle]
    unsafe extern "C" fn qtfb_destroy_connection(connection: *mut dumbstupidhack::QTFB_ClientConnection) {
        if connection.is_null() {
            return;
        }
        let _ = Box::from_raw(connection);
    }
    #[no_mangle]
    extern "C" fn qtfb_get_socket_path() -> *const libc::c_char {
        QTFB_SOCKET_PATH.as_ptr() as *const libc::c_char
    }

    #[no_mangle]
    unsafe extern "C" fn qtfb_get_buffer(connection: *mut dumbstupidhack::QTFB_ClientConnection) -> *const u8 {
        return connection.as_ref().unwrap().shm.as_ptr()
    }

    #[no_mangle]
    unsafe extern "C" fn qtfb_poll_input(connection: *mut dumbstupidhack::QTFB_ClientConnection, message: *const dumbstupidhack::QTFB_InputMessage) -> bool {
        match (connection.as_ref().unwrap().poll_input()) {
            Ok(val) => {
                let dst_ptr = message as *mut qtfb_client::InputMessage;
                std::ptr::write(dst_ptr, val);
                true
            },
            Err(_) => false
        }
    }
}