#ifndef POSTGRES_DB_H
#define POSTGRES_DB_H

#include <libpq-fe.h>
#include "../common/protocol.h"

// =========================================
// DATABASE CONNECTION MANAGEMENT
// =========================================
int db_init(const char* conninfo);
void db_cleanup();
PGconn* db_get_connection();

// =========================================
// USER MANAGEMENT
// =========================================
int db_create_user(const char* username, const char* password, const char* email);
int db_find_user_by_username(const char* username, int* user_id, char* email, int email_size, int* is_active);
int db_find_user_by_id(int user_id, char* username, int username_size, char* email, int email_size, int* is_active);
int db_verify_password(const char* username, const char* password);
int db_validate_username(const char* username);
int db_validate_email(const char* email);
int db_update_user_status(int user_id, int is_active);

// =========================================
// SESSION MANAGEMENT
// =========================================
int db_create_session(int user_id, char* session_token, int token_size);
int db_validate_session(const char* session_token, int* user_id);
int db_delete_session(const char* session_token);
int db_delete_user_sessions(int user_id);

// =========================================
// FRIEND MANAGEMENT
// Chức năng: Hiển thị danh sách bạn bè
// =========================================
int db_send_friend_request(int sender_id, int receiver_id);
int db_accept_friend_request(int request_id);
int db_reject_friend_request(int request_id);
int db_cancel_friend_request(int request_id);
int db_remove_friend(int user_id, int friend_id);
int db_get_friend_requests(int user_id, char*** results, int* count);

/**
 * Chức năng : Lấy danh sách bạn bè của user
 * @param user_id - ID của user cần lấy danh sách bạn bè
 * @param results - Mảng kết quả chứa thông tin bạn bè (cần free sau khi dùng)
 * @param count - Số lượng bạn bè
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_get_friends_list(int user_id, char*** results, int* count);

int db_check_friendship(int user_id1, int user_id2);

// =========================================
// EVENT MANAGEMENT
// Chức năng : Quản lý sự kiện
// =========================================

/**
 * Chức năng: Tạo mới sự kiện
 * @param creator_id - ID của user tạo sự kiện
 * @param event_name - Tên sự kiện
 * @param description - Mô tả sự kiện
 * @param location - Địa điểm tổ chức
 * @param start_time - Thời gian bắt đầu (format: YYYY-MM-DD HH:MM:SS)
 * @param end_time - Thời gian kết thúc (format: YYYY-MM-DD HH:MM:SS)
 * @param max_participants - Số lượng người tham gia tối đa
 * @return event_id nếu thành công, -1 nếu lỗi
 */
int db_create_event(int creator_id,const char* event_name,const char* description,const char* location,
                    const char* event_time,const char* event_type);

/**
 * Chức năng: Sửa thông tin sự kiện
 * @param event_id - ID của sự kiện cần sửa
 * @param event_name - Tên sự kiện mới
 * @param description - Mô tả mới
 * @param location - Địa điểm mới
 * @param start_time - Thời gian bắt đầu mới
 * @param end_time - Thời gian kết thúc mới
 * @param max_participants - Số lượng người tham gia tối đa mới
 * @return 0 nếu thành công, -1 nếu lỗi
 */
// return: 1 = updated, 0 = not found, -1 = db error
int db_update_event(int creator_id, int event_id,const char* title,const char* description,
                    const char* location,const char* event_time,const char* event_type);

/**
 * Chức năng: Xóa sự kiện
 * @param event_id - ID của sự kiện cần xóa
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_delete_event(int user_id,int event_id);

int db_get_event_details(int event_id, char*** results);
int db_get_user_events(int user_id, char*** results, int* count);

/**
 * Chức năng : Lấy danh sách tất cả sự kiện
 * @param results - Mảng kết quả chứa thông tin sự kiện (cần free sau khi dùng)
 * @param count - Số lượng sự kiện
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_get_all_events(char*** results, int* count);

// Lấy chi tiết sự kiện theo người tạo và event_id
int db_get_event_detail_by_creator(int user_id, int event_id, char** out_extra);


int db_search_events(const char* keyword, char*** results, int* count);

// =========================================
// EVENT PARTICIPANTS MANAGEMENT
// =========================================
int db_join_event(int user_id, int event_id);
int db_leave_event(int user_id, int event_id);
int db_get_event_participants(int event_id, char*** results, int* count);
int db_check_participant(int user_id, int event_id);

// =========================================
// EVENT INVITATIONS
// Chức năng : Lời mời tham gia sự kiện
// =========================================

/**
 * Chức năng : Gửi lời mời tham gia sự kiện
 * @param event_id - ID của sự kiện
 * @param inviter_id - ID của người gửi lời mời
 * @param invitee_id - ID của người được mời
 * @return invitation_id nếu thành công, -1 nếu lỗi
 */
int db_send_event_invitation(int event_id, int inviter_id, int invitee_id);

/**
 * Chức năng : Chấp nhận lời mời tham gia sự kiện
 * @param invitation_id - ID của lời mời
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_accept_event_invitation(int invitation_id);

int db_reject_event_invitation(int invitation_id);
int db_get_user_invitations(int user_id, char*** results, int* count);

// =========================================
// EVENT JOIN REQUESTS
// Chức năng : Yêu cầu tham gia sự kiện public
// =========================================

/**
 * Chức năng : Tạo yêu cầu tham gia sự kiện public
 * @param user_id - ID của user muốn tham gia
 * @param event_id - ID của sự kiện
 * @return request_id nếu thành công, -1 nếu lỗi
 */
int db_create_join_request(int user_id, int event_id);

/**
 * Chức năng : Chấp nhận yêu cầu tham gia sự kiện
 * @param request_id - ID của yêu cầu tham gia
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_approve_join_request(int request_id);

int db_reject_join_request(int request_id);
int db_get_event_join_requests(int event_id, char*** results, int* count);

// =========================================
// ACTIVITY LOGS
// Chức năng : Ghi log hoạt động
// =========================================

/**
 * Chức năng : Ghi log hoạt động của user
 * @param user_id - ID của user
 * @param activity_type - Loại hoạt động (VD: "CREATE_EVENT", "JOIN_EVENT", "SEND_INVITATION")
 * @param details - Chi tiết hoạt động
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_log_activity(int user_id, const char* activity_type, const char* details);

int db_get_user_activities(int user_id, int limit, char*** results, int* count);

// =========================================
// UTILITY FUNCTIONS
// =========================================
void db_free_results(char*** results, int count);
char* db_generate_session_token();

#endif // POSTGRES_DB_H
