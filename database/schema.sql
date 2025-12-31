-- =========================================
-- Event Management System Database Schema
-- PostgreSQL
-- =========================================

-- Drop existing tables
DROP TABLE IF EXISTS event_join_requests CASCADE;
DROP TABLE IF EXISTS event_invitations CASCADE;
DROP TABLE IF EXISTS event_participants CASCADE;
DROP TABLE IF EXISTS events CASCADE;
DROP TABLE IF EXISTS friendships CASCADE;
DROP TABLE IF EXISTS friend_requests CASCADE;
DROP TABLE IF EXISTS activity_logs CASCADE;
DROP TABLE IF EXISTS users CASCADE;

-- =========================================
-- 1. USERS TABLE - Quản lý tài khoản
-- =========================================
CREATE TABLE users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password VARCHAR(255) NOT NULL,  
    email VARCHAR(100) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'inactive', 'banned'))
);

-- Index cho tìm kiếm nhanh
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email ON users(email);

-- =========================================
-- 2. FRIEND_REQUESTS TABLE - Lời mời kết bạn
-- =========================================
CREATE TABLE friend_requests (
    request_id SERIAL PRIMARY KEY,
    sender_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    receiver_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'accepted', 'rejected')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    responded_at TIMESTAMP,
    UNIQUE(sender_id, receiver_id)
);

CREATE INDEX idx_friend_requests_receiver ON friend_requests(receiver_id, status);
CREATE INDEX idx_friend_requests_sender ON friend_requests(sender_id);

-- =========================================
-- 3. FRIENDSHIPS TABLE - Danh sách bạn bè
-- =========================================
CREATE TABLE friendships (
    friendship_id SERIAL PRIMARY KEY,
    user1_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    user2_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CHECK (user1_id < user2_id),  -- Đảm bảo user1_id < user2_id để tránh trùng lặp
    UNIQUE(user1_id, user2_id)
);
CREATE INDEX idx_friendships_user1 ON friendships(user1_id);
CREATE INDEX idx_friendships_user2 ON friendships(user2_id);

-- =========================================
-- 4. EVENTS TABLE - Quản lý sự kiện
-- =========================================
CREATE TABLE events (
    event_id SERIAL PRIMARY KEY, -- Mã sự kiện
    creator_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE, -- Người tạo sự kiện
    title VARCHAR(200) NOT NULL, -- Tiêu đề sự kiện
    description TEXT, --Mô tả sự kiện
    location VARCHAR(255), -- Địa điểm tổ chức
    event_time TIMESTAMP NOT NULL,-- Thời gian diễn ra sự kiện
    event_type VARCHAR(20) NOT NULL CHECK (event_type IN ('private', 'public')), -- Loại sự kiện
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'cancelled', 'completed')), -- Trạng thái sự kiện
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- Thời gian tạo sự kiện
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- Thời gian cập nhật sự kiện
);

-- =========================================
-- 5. EVENT_PARTICIPANTS TABLE - Người tham gia sự kiện
-- =========================================
CREATE TABLE event_participants (
    participant_id SERIAL PRIMARY KEY,
    event_id INTEGER NOT NULL REFERENCES events(event_id) ON DELETE CASCADE,
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    role VARCHAR(20) DEFAULT 'participant' CHECK (role IN ('creator', 'participant')),
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(event_id, user_id)
);

CREATE INDEX idx_event_participants_event ON event_participants(event_id);
CREATE INDEX idx_event_participants_user ON event_participants(user_id);

-- =========================================
-- 6. EVENT_INVITATIONS TABLE - Lời mời tham gia sự kiện
-- =========================================
CREATE TABLE event_invitations (
    invitation_id SERIAL PRIMARY KEY,
    event_id INTEGER NOT NULL REFERENCES events(event_id) ON DELETE CASCADE,
    sender_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,  -- Người gửi lời mời
    receiver_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,  -- Người nhận
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'accepted', 'rejected')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    responded_at TIMESTAMP,
    UNIQUE(event_id, receiver_id)
);

CREATE INDEX idx_event_invitations_receiver ON event_invitations(receiver_id, status);
CREATE INDEX idx_event_invitations_event ON event_invitations(event_id);

-- =========================================
-- 7. EVENT_JOIN_REQUESTS TABLE - Yêu cầu tham gia sự kiện Public
-- =========================================
CREATE TABLE event_join_requests (
    join_request_id SERIAL PRIMARY KEY,
    event_id INTEGER NOT NULL REFERENCES events(event_id) ON DELETE CASCADE,
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'accepted', 'rejected')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    responded_at TIMESTAMP,
    UNIQUE(event_id, user_id)
);

CREATE INDEX idx_event_join_requests_event ON event_join_requests(event_id, status);
CREATE INDEX idx_event_join_requests_user ON event_join_requests(user_id);

-- =========================================
-- 8. ACTIVITY_LOGS TABLE - Ghi log hoạt động
-- =========================================
CREATE TABLE activity_logs (
    log_id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(user_id) ON DELETE SET NULL,
    action VARCHAR(100) NOT NULL,  -- 'login', 'logout', 'create_event', 'send_friend_request', etc.
    details TEXT,  -- JSON hoặc text mô tả chi tiết
    ip_address VARCHAR(45),  -- Hỗ trợ IPv6
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_activity_logs_user ON activity_logs(user_id);
CREATE INDEX idx_activity_logs_action ON activity_logs(action);
CREATE INDEX idx_activity_logs_time ON activity_logs(created_at);

-- =========================================
-- VIEWS - Các view tiện ích
-- =========================================

-- View: Danh sách bạn bè của user
CREATE OR REPLACE VIEW user_friends AS
SELECT 
    f.friendship_id,
    f.user1_id as user_id,
    f.user2_id as friend_id,
    u.username as friend_username,
    u.email as friend_email,
    f.created_at
FROM friendships f
JOIN users u ON f.user2_id = u.user_id
UNION ALL
SELECT 
    f.friendship_id,
    f.user2_id as user_id,
    f.user1_id as friend_id,
    u.username as friend_username,
    u.email as friend_email,
    f.created_at
FROM friendships f
JOIN users u ON f.user1_id = u.user_id;

-- View: Danh sách sự kiện của user
CREATE OR REPLACE VIEW user_events AS
SELECT 
    e.event_id,
    e.title,
    e.description,
    e.location,
    e.event_time,
    e.event_type,
    e.status,
    e.creator_id,
    u.username as creator_username,
    ep.user_id as participant_id,
    ep.role
FROM events e
JOIN users u ON e.creator_id = u.user_id
LEFT JOIN event_participants ep ON e.event_id = ep.event_id;

-- =========================================
-- FUNCTIONS - Các hàm tiện ích
-- =========================================

-- Function: Kiểm tra 2 user có phải bạn bè không
CREATE OR REPLACE FUNCTION are_friends(uid1 INTEGER, uid2 INTEGER)
RETURNS BOOLEAN AS $$
BEGIN
    RETURN EXISTS (
        SELECT 1 FROM friendships 
        WHERE (user1_id = LEAST(uid1, uid2) AND user2_id = GREATEST(uid1, uid2))
    );
END;
$$ LANGUAGE plpgsql;

-- =========================================
-- TRIGGERS
-- =========================================

-- Trigger: Tự động update updated_at khi sửa event
CREATE OR REPLACE FUNCTION update_event_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_update_event_timestamp
BEFORE UPDATE ON events
FOR EACH ROW
EXECUTE FUNCTION update_event_timestamp();

-- Trigger: Tự động thêm creator vào participants khi tạo event
CREATE OR REPLACE FUNCTION add_creator_to_participants()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO event_participants (event_id, user_id, role)
    VALUES (NEW.event_id, NEW.creator_id, 'creator');
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_add_creator_to_participants
AFTER INSERT ON events
FOR EACH ROW
EXECUTE FUNCTION add_creator_to_participants();

