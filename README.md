# TCP Socket Social Network Project

Đồ án môn Lập trình mạng - IT4062

## 🎯 Mô tả

Project này là một hệ thống mạng xã hội đơn giản sử dụng:
- **Protocol:** TCP Socket với định dạng `COMMAND|JSON\r\n` 
- **JSON Library:** cJSON (industry standard)
- **Storage:** File-based (.txt files) trong thư mục `data/`
- **Architecture:** Multi-threaded concurrent server (pthread)
- **Platform:** Ubuntu/Linux

## ✨ Các chức năng đã implement

✅ **1. Xử lý truyền dòng (Stream Processing)**
- Delimiter-based protocol: `\r\n` 
- Format: `COMMAND_TYPE|JSON_DATA\r\n`
- Buffer overflow protection
- File: `common/protocol.c`, `common/protocol.h`

✅ **2. Cơ chế vào/ra socket server**
- Multi-threaded concurrent server (pthread)
- Thread-per-connection model
- Tự động cleanup session khi client disconnect
- File: `server/server.c`

✅ **3. Đăng ký tài khoản**
- Validation username (không chứa ký tự đặc biệt)
- Kiểm tra username trùng lặp
- Lưu thông tin vào file `data/users.txt`
- File: `server/handlers.c`, `server/file_db.c`

✅ **4. Đăng nhập + quản lý phiên**
- Session token (random 32-char string)
- Thread-safe với pthread_mutex
- Session timeout: 1 giờ
- Mapping user với socket connection
- File: `server/session.c`, `server/handlers.c`

✅ **5. Gửi lời mời kết bạn**
- Validate session token
- Real-time notification cho target user
- Lưu friend request vào `data/friend_requests.txt`
- File: `server/handlers.c` - `handle_friend_invite()`

✅ **6. Chấp nhận/Từ chối lời mời kết bạn**
- Parse boolean `accept` field với cJSON
- Real-time notification cho requester
- Tự động tạo friendship khi accept
- File: `server/handlers.c` - `handle_friend_response()`

✅ **7. Hủy kết bạn**
- Xóa friendship từ `data/friendships.txt`
- Real-time notification cho friend bị xóa
- File: `server/handlers.c` - `handle_friend_remove()`

## 🔧 JSON Library: cJSON

Project đã migrate từ json_helper (custom) sang **cJSON** (industry standard):

**Advantages:**
- ✅ No buffer overflow risk (dynamic allocation)
- ✅ Support nested JSON & arrays
- ✅ Better error handling
- ✅ Industry standard (thousands of projects use it)
- ✅ Active maintenance & documentation

**Migration details:** See `MIGRATION_SUMMARY.md`

## Cấu trúc thư mục

```
IT4062-Project/
├── common/              # Shared code
│   ├── protocol.h       # Protocol definitions
│   ├── protocol.c       # Send/receive message functions
│   ├── json_helper.h    # JSON parsing helpers
│   └── json_helper.c
├── server/              # Server code
│   ├── server.c        # Server main loop
│   ├── server.h        # Server headers
│   ├── handlers.c      # Request handlers
│   ├── file_db.h       # File database definitions
│   ├── file_db.c       # File database operations
│   ├── session.h       # Session management
│   └── session.c
├── client/              # Client code
│   └── client.c        # Client application
├── data/                # Data files (tạo tự động)
│   ├── users.txt       # User data
│   ├── friend_requests.txt  # Friend requests
│   └── friendships.txt      # Friendships
├── build/               # Build output (tạo tự động)
├── Makefile            # Build script
└── README.md           # This file
```

## Yêu cầu hệ thống

- Ubuntu/Linux OS
- GCC compiler
- pthread library
- Make utility

## Hướng dẫn build

### Build tất cả

```bash
make
```

Lệnh này sẽ tạo:
- `build/server`
- `build/client`
- `data/` directory

### Build riêng lẻ

```bash
make build/server  # Build server only
make build/client  # Build client only
```

### Clean build files

```bash
make clean      # Xóa build files
make clean-all  # Xóa build và data files
```

## Hướng dẫn chạy

### 1. Chạy Server

Mở terminal và chạy:

```bash
make run-server
```

Hoặc:

```bash
./build/server
```

Server sẽ lắng nghe trên port **8888**

### 2. Chạy Client

Mở terminal khác và chạy:

```bash
make run-client
```

Hoặc:

```bash
./build/client
```

## Hướng dẫn sử dụng Client

### Menu chính

```
=== MENU ===
1. Register           # Đăng ký tài khoản
2. Login             # Đăng nhập
3. Send friend request    # Gửi lời mời kết bạn
4. Accept friend request  # Chấp nhận lời mời
5. Reject friend request  # Từ chối lời mời
6. Remove friend          # Hủy kết bạn
0. Exit                   # Thoát
```

### Demo workflow

#### 1. Đăng ký tài khoản (Client A)

```
Chọn: 1
Username: ducanh
Password: 123456
Email: ducanh@example.com
```

#### 2. Đăng ký tài khoản (Client B)

```
Chọn: 1
Username: hoang
Password: 123456
Email: hoang@example.com
```

#### 3. Đăng nhập (Client A)

```
Chọn: 2
Username: ducanh
Password: 123456
```

#### 4. Đăng nhập (Client B)

```
Chọn: 2
Username: hoang
Password: 123456
```

#### 5. Gửi lời mời kết bạn (Client A → Client B)

```
Client A:
Chọn: 3
Target username: hoang
```

**Client B sẽ nhận notification:**
```
=== NEW FRIEND REQUEST ===
From: ducanh
Request ID: 1
========================
```

#### 6. Chấp nhận lời mời (Client B)

```
Client B:
Chọn: 4
Request ID: 1
```

**Client A sẽ nhận notification:**
```
=== FRIEND REQUEST ACCEPTED ===
User 'hoang' accepted your friend request!
==============================
```

#### 7. Hủy kết bạn (Client A)

```
Client A:
Chọn: 6
Friend username: hoang
```

**Client B sẽ nhận notification:**
```
=== FRIENDSHIP REMOVED ===
User 'ducanh' removed you as a friend
========================
```

## Giao thức chi tiết

### Format chung

```
COMMAND_TYPE | JSON_DATA
```

### 1. REGISTER

**Client → Server:**
```
REGISTER|{"username": "ducanh", "password": "123456", "email": "test@example.com"}
```

**Server → Client:**
```
RES_REGISTER|{"code": 201, "message": "Account created"}
```

### 2. LOGIN

**Client → Server:**
```
LOGIN|{"username": "ducanh", "password": "123456"}
```

**Server → Client:**
```
RES_LOGIN|{"code": 200, "session_token": "abcxyz123", "user_id": 1}
```

### 3. FRIEND_INVITE

**Client → Server:**
```
FRIEND_INVITE|{"session_token": "abc123", "target_user": "hoang"}
```

**Server → Client (response):**
```
RES_FRIEND_INVITE|{"code": 111, "message": "Friend request sent", "request_id": 1}
```

**Server → Target User (notification):**
```
FRIEND_INVITE_NOTIFICATION|{"code": 300, "from_user": "ducanh", "from_user_id": 1, "request_id": 1, "message": "You have a new friend request"}
```

### 4. FRIEND_RESPONSE (Accept/Reject)

**Client → Server:**
```
FRIEND_RESPONSE|{"session_token": "abc123", "request_id": 1, "accept": true}
```

**Server → Client (response):**
```
RES_FRIEND_RESPONSE|{"code": 112, "message": "Friend request accepted"}
```

**Server → Requester (notification):**
```
FRIEND_ACCEPTED_NOTIFICATION|{"code": 301, "from_user_id": 2, "from_user": "hoang", "message": "Your friend request was accepted"}
```

### 5. FRIEND_REMOVE

**Client → Server:**
```
FRIEND_REMOVE|{"session_token": "abc123", "friend_username": "hoang"}
```

**Server → Client (response):**
```
RES_FRIEND_REMOVE|{"code": 113, "message": "Friend removed"}
```

**Server → Removed Friend (notification):**
```
FRIEND_REMOVED_NOTIFICATION|{"code": 302, "from_user": "ducanh", "from_user_id": 1, "message": "You are no longer friends"}
```

## Response Codes

| Code | Ý nghĩa |
|------|---------|
| 200 | Thành công (Login) |
| 201 | Tạo thành công (Register) |
| 111 | Friend request sent |
| 112 | Friend request accepted |
| 113 | Friend request declined / Friend removed |
| 300 | Friend invite notification |
| 301 | Friend accepted notification |
| 302 | Friend removed notification |
| 400 | Missing required fields |
| 401 | Invalid session / Wrong password |
| 404 | User not found |
| 409 | Username already exists |
| 422 | Invalid username (special characters) |
| 500 | Unknown error |

## Kiến trúc hệ thống

### Server Side

- **Multi-threaded**: Mỗi client connection chạy trong pthread riêng
- **File-based Database**: Lưu users, friend requests, friendships trong file .txt
- **Session Management**: Token-based authentication với timeout
- **Real-time Notifications**: Push notifications qua socket connection

### Client Side

- **Two-threaded**: 
  - Main thread: Xử lý input và gửi request
  - Receive thread (pthread): Lắng nghe notifications từ server
- **Interactive Console UI**: Menu-driven interface

## Features kỹ thuật

### 1. Stream Processing (Xử lý truyền dòng)

- Gửi độ dài message (4 bytes) trước nội dung
- Xử lý partial sends/receives
- Buffer management

### 2. Protocol Design

- Delimiter-based: `COMMAND_TYPE | JSON_DATA`
- Simple JSON parsing (không dùng external library)
- Extensible command system

### 3. Concurrency

- Thread-safe file operations
- Session management với concurrent access
- Proper cleanup khi client disconnect
- POSIX threads (pthread)

### 4. Error Handling

- Comprehensive error codes
- Validation ở multiple layers
- Graceful degradation

## Testing

### Test case 1: Register & Login

1. Chạy server
2. Chạy 2 client
3. Đăng ký 2 tài khoản khác nhau
4. Đăng nhập cả 2

**Expected**: Cả 2 đều nhận được session token

### Test case 2: Friend Request Flow

1. Client A gửi friend request cho B
2. Client B nhận notification
3. Client B accept
4. Client A nhận notification

**Expected**: Cả 2 trở thành friends

### Test case 3: Concurrent Requests

1. Client A và B cùng gửi friend request cho nhau
2. Cả 2 accept

**Expected**: Chỉ tạo 1 friendship duy nhất

### Test case 4: Invalid Operations

1. Gửi friend request mà không login
2. Accept request không tồn tại
3. Remove friend chưa kết bạn

**Expected**: Nhận error codes phù hợp

## Mở rộng (dành cho bạn của bạn)

Các chức năng còn lại cần implement (8-17):
- Lấy danh sách sự kiện
- Tạo/sửa/xóa sự kiện
- Gửi lời mời tham gia sự kiện
- Chấp nhận lời mời
- Yêu cầu tham gia sự kiện public
- Chấp nhận yêu cầu tham gia
- Ghi log hoạt động
- Hiển thị danh sách bạn bè

### Gợi ý implementation:

1. Thêm Event structure vào `file_db.h`
2. Thêm command types mới vào `protocol.h`
3. Implement handlers tương tự như friend handlers
4. Thêm menu options vào client
5. Tạo file `data/events.txt` để lưu events

## File Database Format

### users.txt
```
user_id|username|password|email|is_active
1|ducanh|123456|ducanh@example.com|1
2|hoang|123456|hoang@example.com|1
```

### friend_requests.txt
```
request_id|from_user_id|to_user_id|status|created_at
1|1|2|0|1732611234
```
- status: 0 = pending, 1 = accepted, 2 = rejected

### friendships.txt
```
user1_id|user2_id|created_at
1|2|1732611345
```

## Troubleshooting

### Lỗi: "Permission denied" khi chạy
```bash
chmod +x build/server
chmod +x build/client
```

### Lỗi: "Address already in use"
- Port 8888 đang bị sử dụng
- Đợi vài giây hoặc kill process đang dùng port
```bash
sudo lsof -i :8888
sudo kill -9 <PID>
```

### Lỗi: "Connection refused"
- Đảm bảo server đang chạy
- Kiểm tra firewall settings
```bash
sudo ufw allow 8888
```

### Client không nhận notification
- Đảm bảo đã login thành công
- Kiểm tra receive thread đang chạy
- Xem server logs

### Lỗi compilation
```bash
# Cài đặt build tools
sudo apt update
sudo apt install build-essential
```

## Tác giả

- Sinh viên IT4062
- Năm học: 2025

## License

Educational project - IT4062 Network Programming

---

**Chúc bạn code thành công! 🚀**
