use anyhow::{Error, Result, anyhow, bail};
use libc::{
    c_void, mmap, munmap, sockaddr_un, socket, AF_UNIX, MAP_FAILED, MAP_SHARED, PROT_READ,
    PROT_WRITE, SOCK_SEQPACKET,
};
use std::ffi::CString;
use std::fs::OpenOptions;
use std::io;
use std::mem;
use std::mem::transmute;
use std::os::unix::io::{AsRawFd, RawFd};
use std::ptr;
use std::slice;

pub mod constants {
    pub const SOCKET_PATH: &str = "/tmp/qtfb.sock";
    
    pub const MESSAGE_INITIALIZE: u8 = 0;
    pub const MESSAGE_UPDATE: u8 = 1;
    pub const MESSAGE_CUSTOM_INITIALIZE: u8 = 2;
    pub const MESSAGE_TERMINATE: u8 = 3;
    pub const MESSAGE_USER_INPUT: u8 = 4;
    pub const MESSAGE_SET_REFRESH_MODE: u8 = 5;
    pub const MESSAGE_REQUEST_FULL_REFRESH: u8 = 6;
    pub const MESSAGE_DEVICE_STATE_CHANGED: u8 = 7;
    pub const MESSAGE_DEVICE_STATE_INIT: u8 = 8;

    pub const STATE_CHANGED_REASON_ROTATION: i32 = 0;

    pub const UPDATE_ALL: i32 = 0;
    pub const UPDATE_PARTIAL: i32 = 1;

    pub const FBFMT_RM2FB: u8 = 0;
    pub const FBFMT_RMPP_RGB888: u8 = 1;
    pub const FBFMT_RMPP_RGBA8888: u8 = 2;

    #[repr(i32)]
    #[derive(Debug, Clone, Copy)]
    pub enum RefreshMode {
        Ufast = 0,
        Fast = 1,
        Animate = 2,
        Content = 3,
        Ui = 4,
    }


    pub type FBKey = u32;
}

pub type FBKey = u32;

#[repr(C)]
#[derive(Clone, Copy)]
struct InitMessageContents {
    framebuffer_key: FBKey,
    framebuffer_type: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct CustomInitMessageContents {
    framebuffer_key: FBKey,
    framebuffer_type: u8,
    width: u16,
    height: u16,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct InitMessageResponseContents {
    shm_key_defined: i32,
    shm_size: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct UpdateRegionMessageContents {
    msg_type: i32,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct UserInputContents {
    input_type: i32,
    dev_id: i32,
    x: i32,
    y: i32,
    d: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
union RotationDeviceStateChangeContents {
    rotation: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
union DeviceStateChangeContentsData {
    rotation: RotationDeviceStateChangeContents,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct DeviceStateChangeContents {
    reason: i32,
    data: DeviceStateChangeContentsData,
}

#[derive(Clone, Copy, Debug)]
struct Null;

#[repr(C)]
union ClientMessageContents {
    init: InitMessageContents,
    update: UpdateRegionMessageContents,
    custom_init: CustomInitMessageContents,
    refresh_mode: constants::RefreshMode,
    null: Null,
}

#[repr(C)]
struct ClientMessage {
    msg_type: u8,
    contents: ClientMessageContents,
}

#[repr(C)]
union ServerMessageContents {
    init: InitMessageResponseContents,
    user_input: UserInputContents,
    device_state_change: DeviceStateChangeContents,
}

#[derive(Clone, Debug)]
pub enum DeviceStateChange {
    Rotation{ rotation: i32 },
}

#[derive(Clone, Debug)]
pub enum UserFacingServerMessageContents {
    UserInput(UserInputContents),
    DeviceStateChange(DeviceStateChange),
}

#[repr(C)]
struct ServerMessage {
    msg_type: u8,
    contents: ServerMessageContents,
}

pub struct ClientConnection<'a> {
    fd: RawFd,
    pub shm: &'a mut [u8],
}

fn parse_as_device_state_change(contents: &ServerMessageContents) -> Result<UserFacingServerMessageContents>{
    let dsc: &DeviceStateChangeContents = unsafe { &contents.device_state_change };
    match dsc.reason {
        constants::STATE_CHANGED_REASON_ROTATION => {
            Ok(UserFacingServerMessageContents::DeviceStateChange(
                DeviceStateChange::Rotation{ rotation: unsafe { dsc.data.rotation.rotation } }
            ))
        },
        other => bail!("Unknown device state change reason: {other}"),
    }
}

impl<'a> ClientConnection<'a> {
    pub fn new(
        framebuffer_id: FBKey,
        shm_type: u8,
        custom_resolution: Option<(u16, u16)>,
    ) -> Result<Self> {
        let fd = unsafe { socket(AF_UNIX, SOCK_SEQPACKET, 0) };
        if fd == -1 {
            return Err(Error::new(io::Error::last_os_error()));
        }

        let mut addr = sockaddr_un {
            sun_family: AF_UNIX as u16,
            sun_path: [0; 108],
        };
        let socket_path = CString::new(constants::SOCKET_PATH).unwrap();
        let path_bytes = socket_path.as_bytes_with_nul();
        addr.sun_path[..path_bytes.len()].copy_from_slice(unsafe {
            std::slice::from_raw_parts(transmute(path_bytes.as_ptr()), path_bytes.len())
        });

        let connect_res = unsafe {
            libc::connect(
                fd,
                &addr as *const _ as *const libc::sockaddr,
                mem::size_of::<sockaddr_un>() as u32,
            )
        };

        if connect_res != 0 {
            return Err(Error::new(io::Error::last_os_error()));
        }

        let init_message = if let Some((width, height)) = custom_resolution {
            ClientMessage {
                msg_type: constants::MESSAGE_CUSTOM_INITIALIZE,
                contents: ClientMessageContents {
                    custom_init: CustomInitMessageContents {
                        framebuffer_key: framebuffer_id,
                        framebuffer_type: shm_type,
                        width,
                        height,
                    },
                },
            }
        } else {
            ClientMessage {
                msg_type: constants::MESSAGE_INITIALIZE,
                contents: ClientMessageContents {
                    init: InitMessageContents {
                        framebuffer_key: framebuffer_id,
                        framebuffer_type: shm_type,
                    },
                },
            }
        };

        let send_res = unsafe {
            libc::send(
                fd,
                &init_message as *const _ as *const c_void,
                mem::size_of::<ClientMessage>(),
                0,
            )
        };

        if send_res == -1 {
            return Err(Error::new(io::Error::last_os_error()));
        }

        let mut server_message = ServerMessage {
            msg_type: 0,
            contents: ServerMessageContents {
                init: InitMessageResponseContents {
                    shm_key_defined: 0,
                    shm_size: 0,
                },
            }
        };

        let recv_res = unsafe {
            libc::recv(
                fd,
                &mut server_message as *mut _ as *mut c_void,
                mem::size_of::<ServerMessage>(),
                0,
            )
        };

        if recv_res < 1 {
            return Err(Error::new(io::Error::last_os_error()));
        }

        let shm_name = unsafe {
            format!("/dev/shm/qtfb_{}", server_message.contents.init.shm_key_defined)
        };

        let shm_fd = OpenOptions::new().read(true).write(true).open(&shm_name)?;

        let shm_ptr = unsafe {
            mmap(
                ptr::null_mut(),
                server_message.contents.init.shm_size,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                shm_fd.as_raw_fd(),
                0,
            )
        };

        if shm_ptr == MAP_FAILED {
            return Err(Error::new(io::Error::last_os_error()));
        }

        let shm =
            unsafe {slice::from_raw_parts_mut(shm_ptr as *mut u8, server_message.contents.init.shm_size) };

        Ok(Self { fd, shm })
    }

    pub fn poll_server_message(&self) -> Result<UserFacingServerMessageContents> {
        let mut server_message = ServerMessage {
            msg_type: 0,
            contents: ServerMessageContents {
                init: InitMessageResponseContents {
                    shm_key_defined: 0,
                    shm_size: 0,
                },
            }
        };

        let recv_res = unsafe {
            libc::recv(
                self.fd,
                &mut server_message as *mut _ as *mut c_void,
                mem::size_of::<ServerMessage>(),
                0,
            )
        };

        if recv_res < 1 {
            return Err(Error::new(io::Error::last_os_error()));
        }

        unsafe {
            match server_message.msg_type {
                constants::MESSAGE_USER_INPUT => Ok(
                    UserFacingServerMessageContents::UserInput(
                        server_message.contents.user_input
                    )
                ),
                constants::MESSAGE_DEVICE_STATE_INIT => parse_as_device_state_change(&server_message.contents),
                constants::MESSAGE_DEVICE_STATE_CHANGED => parse_as_device_state_change(&server_message.contents),
                msg_type => Err(anyhow!("Unknown server message type {msg_type}"))
            }
        }
    }

    pub fn send_complete_update(&self) -> io::Result<()> {
        let update_message = ClientMessage {
            msg_type: constants::MESSAGE_UPDATE,
            contents: ClientMessageContents {
                update: UpdateRegionMessageContents {
                    msg_type: constants::UPDATE_ALL,
                    x: 0,
                    y: 0,
                    w: 0,
                    h: 0,
                },
            },
        };

        self.send_message(&update_message)
    }

    pub fn send_partial_update(&self, x: i32, y: i32, w: i32, h: i32) -> io::Result<()> {
        let update_message = ClientMessage {
            msg_type: constants::MESSAGE_UPDATE,
            contents: ClientMessageContents {
                update: UpdateRegionMessageContents {
                    msg_type: constants::UPDATE_PARTIAL,
                    x,
                    y,
                    w,
                    h,
                },
            },
        };

        self.send_message(&update_message)
    }

    pub fn request_full_request(&self) -> io::Result<()> {
        self.send_message(&ClientMessage {
            msg_type: constants::MESSAGE_REQUEST_FULL_REFRESH,
            contents: ClientMessageContents{ null: Null }
        })
    }

    pub fn set_refresh_mode(&self, refresh_mode: constants::RefreshMode) -> io::Result<()> {
        self.send_message(&ClientMessage {
            msg_type: constants::MESSAGE_SET_REFRESH_MODE,
            contents: ClientMessageContents{
                refresh_mode,
            }
        })
    }

    fn send_message(&self, msg: &ClientMessage) -> io::Result<()> {
        let res = unsafe {
            libc::send(
                self.fd,
                msg as *const _ as *const c_void,
                mem::size_of::<ClientMessage>(),
                0,
            )
        };

        if res == -1 {
            return Err(io::Error::last_os_error());
        }
        Ok(())
    }
}

impl<'a> Drop for ClientConnection<'a> {
    fn drop(&mut self) {
        unsafe {
            munmap(self.shm.as_ptr() as *mut c_void, self.shm.len());
            libc::close(self.fd);
        }
    }
}
