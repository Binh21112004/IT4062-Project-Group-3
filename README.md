# TCP Socket Server Project

**Đồ án môn Lập trình mạng - IT4062**

---

## 1. 🎯 Tổng quan dự án

### Mô tả
Hệ thống TCP socket server đa luồng với xác thực người dùng, session management và giao thức custom delimiter-based.

### Kỹ thuật sử dụng

#### **1.1 Lập trình mạng (Network Programming)**
- **TCP Socket** với `socket()`, `bind()`, `listen()`, `accept()`, `send()`, `recv()`
- **Đa luồng (Multi-threading)** - Mô hình thread-per-connection sử dụng `pthread`
- **Server xử lý đồng thời (Concurrent Server)** - Xử lý đồng thời nhiều clients (tối đa 1000 phiên)

#### **1.2 Thiết kế giao thức (Protocol Design)**
- **Giao thức sử dụng**: `COMMAND|JSON_DATA`
- **Xử lý luồng dữ liệu (Stream Processing)**: Xử lý phân mảnh TCP stream với delimiter `\r\n`
- **Bảo vệ tràn bộ đệm (Buffer Overflow Protection)**: Vòng lặp `recv()` với kiểm tra kích thước
- **Dữ liệu JSON (JSON Payload)**: Sử dụng thư viện cJSON cho tuần tự hóa dữ liệu

#### **1.3 Đồng thời & An toàn luồng (Concurrency & Thread Safety)**
- **Thư viện pthread**: `pthread_create()`, `pthread_detach()`, `pthread_mutex_t`
- **Đồng bộ hóa Mutex**: Bảo vệ dữ liệu phiên chia sẻ
- **Ngăn chặn đăng nhập trùng lặp**: Kiểm tra user đã đăng nhập ở client khác
- **Thao tác an toàn luồng**: Tất cả các thao tác session đều có bảo vệ mutex

#### **1.4 Quản lý phiên (Session Management)**
- **Xác thực dựa trên Token**: Session token ngẫu nhiên 32 ký tự
- **Hết hạn phiên**: Timeout 1 giờ khi không hoạt động
- **Lưu trữ dựa trên mảng**: `session_t sessions[MAX_SESSIONS]` - Truy cập O(1)
- **Xác thực phiên**: Xác minh token cho mọi request đã xác thực

#### **1.5 Lưu trữ dữ liệu (Data Storage)**
- **Cơ sở dữ liệu dựa trên file**: File text phân tách bằng ký tự pipe (`users.txt`)
- **Không dùng cơ sở dữ liệu ngoài**: Lưu trữ nhẹ cho mục đích học tập
- **Định dạng**: `user_id|username|password|email|is_active`
- **Lưu trữ bền vững**: Dữ liệu được lưu vĩnh viễn trên đĩa

#### **1.6 Thư viện & Công cụ (Libraries & Tools)**
- **cJSON**: Thư viện chuẩn công nghiệp để phân tích/tạo JSON
- **pthread**: Thư viện luồng POSIX
- **GCC**: Trình biên dịch GNU C với chuẩn C99
- **Make**: Công cụ tự động hóa build

---

## 2. ✨ Các chức năng hiện có

### **2.1 Xử lý truyền dòng (Stream Processing)** (1 điểm)
**Mô tả:** Xử lý luồng TCP với giao thức dựa trên delimiter

**Kỹ thuật:**
- Delimiter: `\r\n` để phân tách các thông điệp
- Định dạng: `COMMAND|JSON_DATA\r\n`
- Xử lý các thông điệp bị phân mảnh từ TCP stream
- Bảo vệ tràn bộ đệm

**Files:** `common/protocol.c`, `common/protocol.h`

---

### **2.2 Cơ chế vào/ra Socket Server** (2 điểm)
**Mô tả:** Server đồng thời đa luồng

**Kỹ thuật:**
- Mô hình thread-per-connection
- `pthread_create()` tạo thread cho mỗi client
- `pthread_detach()` tự động dọn dẹp
- Chấp nhận nhiều clients đồng thời

**Files:** `server/server.c`

**Ví dụ code:**
```c
while (1) {
    client_sock = accept(server_sock, ...);
    pthread_create(&tid, NULL, client_thread, &client_sock);
    pthread_detach(tid);  // Không chặn
}
```

---

### **2.3 Đăng ký tài khoản (Account Registration)** (2 điểm)
**Mô tả:** Đăng ký tài khoản với xác thực

**Tính năng:**
- Xác thực username (không chứa `|`, `,`, `:`)
- Phát hiện username trùng lặp
- Xác thực email
- Lưu vào file `data/users.txt`

**Files:** `server/server.c` - `handle_register()`, `server/file_db.c`

**Giao thức:**
```json
Yêu cầu: {"username":"alice","password":"pass123","email":"alice@example.com"}
Phản hồi: {"code":100,"message":"Registration successful","user_id":1}
Mã lỗi: 400 (không hợp lệ), 409 (trùng lặp), 500 (lỗi server)
```

---

### **2.4 Đăng nhập + Quản lý phiên (Login + Session Management)** (2 điểm)
**Mô tả:** Xác thực và quản lý phiên đăng nhập

**Tính năng:**
- Xác minh mật khẩu
- Tạo session token ngẫu nhiên 32 ký tự
- Thao tác session an toàn luồng với mutex
- Hết hạn phiên (1 giờ)
- **Ngăn chặn đăng nhập trùng lặp** (từ bài tập cũ)
- Ánh xạ user với kết nối socket

**Files:** `server/session.c`, `server/server.c` - `handle_login()`

**Giao thức:**
```json
Yêu cầu: {"username":"alice","password":"pass123"}
Phản hồi: {"code":200,"session_token":"a1b2c3...","user_id":1}
Mã lỗi: 401 (sai mật khẩu), 404 (không tìm thấy user), 409 (đã đăng nhập)
```

**Cấu trúc Session:**
```c
typedef struct {
    char token[MAX_TOKEN];
    int user_id;
    int client_socket;
    time_t created_at;
    time_t last_activity;
    int is_active;
} Session;
```

---

### **2.5 Đăng xuất (Logout)** (Bonus)
**Mô tả:** Đăng xuất và hủy phiên

**Tính năng:**
- Xác thực session token
- Hủy session khỏi bộ nhớ
- Xóa token phía client
- Ghi log các sự kiện đăng xuất

**Files:** `server/server.c` - `handle_logout()`

**Giao thức:**
```json
Yêu cầu: {"session_token":"a1b2c3..."}
Phản hồi: {"code":200,"message":"Logout successful"}
```

---

## 3. 🚀 Cách chạy dự án

### **3.1 Yêu cầu hệ thống**
- **OS:** Ubuntu/Linux (hoặc WSL trên Windows)
- **Compiler:** GCC with C99+ support
- **Libraries:** pthread, standard C library

**Cài đặt dependencies (Ubuntu):**
```bash
sudo apt-get update
sudo apt-get install build-essential
```

---

### **3.2 Build dự án**

```bash
# Di chuyển đến thư mục dự án

# Xóa build cũ
make clean

# Build server + client
make

# Kiểm tra file thực thi
ls -la build/
# Kết quả: client, server
```

---

### **3.3 Chạy Server**

**Terminal 1:**
```bash
./build/server

# Kết quả mong đợi:
# [SERVER] Database initialized
# [SERVER] Server listening on port 8888...
```

**Logs của Server:**
- `[SERVER]` - Sự kiện server
- `[REGISTER]` - Các lần thử đăng ký
- `[LOGIN]` - Các lần thử đăng nhập
- `[LOGOUT]` - Sự kiện đăng xuất
- `[MESSAGE]` - Lệnh đến

---

### **3.4 Chạy Client**

**Terminal 2 (Client 1):**
```bash
./build/client

# Kết quả:
# === TCP Socket Client ===
# Connecting to server 127.0.0.1:8888...
# Connected successfully!
# 
# === MENU ===
# 1. Register
# 2. Login
# 0. Exit
# ============
# >
```

**Terminal 3 (Client 2):** Mở thêm clients để kiểm tra xử lý đồng thời
```bash
./build/client
```


## 4. 📁 Cấu trúc dự án

```
IT4062-Project/
├── common/              # Định nghĩa các tiện ích
│   ├── protocol.h       # Định nghĩa giao thức (CMD_*, Message struct)
│   ├── protocol.c       # send_message(), receive_message()
│   ├── cJSON.h          # Header thư viện JSON
│   └── cJSON.c          # Triển khai thư viện JSON
│
├── server/              # Code server
│   ├── server.c         # Server chính 
│   ├── server.h         # Headers server
│   ├── file_db.c        # Thao tác cơ sở dữ liệu file 
│   ├── file_db.h        # Cấu trúc database 
│   ├── session.c        # Quản lý phiên 
│   └── session.h        # Cấu trúc session
│
├── client/              # Code client
│   └── client.c         # Ứng dụng client tương tác (257 dòng)
│
├── data/                # Các file dữ liệu (tạo tự động)
│   └── users.txt        # Dữ liệu người dùng (phân cách bằng pipe)
│
├── build/               # Kết quả build (tạo tự động)
│   ├── server           # File thực thi server
│   └── client           # File thực thi client
│
├── Makefile             # Script build
└── README.md            # Mô tả dự án
```

---


## 5. 🔍 Đặc tả các giao thức

### **Message Format**
```
COMMAND|JSON_DATA\r\n
```

### **Supported Commands**

| Command | Direction | Mô tả |
|---------|-----------|-------|
| `REGISTER` | Client → Server | Đăng ký tài khoản |
| `RES_REGISTER` | Server → Client | Response đăng ký |
| `LOGIN` | Client → Server | Đăng nhập |
| `RES_LOGIN` | Server → Client | Response đăng nhập |
| `LOGOUT` | Client → Server | Đăng xuất |
| `RES_LOGOUT` | Server → Client | Response đăng xuất |

### **Response Codes**

| Code | Meaning | Context |
|------|---------|---------|
| 100 | Registration success | REGISTER |
| 200 | Login/Logout success | LOGIN, LOGOUT |
| 400 | Bad request | Invalid JSON or missing fields |
| 401 | Unauthorized | Wrong password, invalid session |
| 404 | Not found | User not found |
| 409 | Conflict | Username exists, already logged in |
| 500 | Server error | Internal error |
| 503 | Service unavailable | Server full (>1000 sessions) |

---



## 6.  Tài liệu tham khảo

- **cJSON:** https://github.com/DaveGamble/cJSON
- **MultiThreading:** Slide môn học

---

