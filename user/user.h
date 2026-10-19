// Giá trị lỗi được trả về khi cấp phát hoặc mở rộng bộ nhớ thất bại.
#define SBRK_ERROR ((char *)-1)

// Khai báo trước để các hàm có thể sử dụng con trỏ tới thông tin tệp.
struct stat;

// Các lời gọi hệ thống được cung cấp cho chương trình người dùng.
// Tạo tiến trình con; tiến trình cha nhận PID của tiến trình con.
int fork(void);
// Kết thúc tiến trình hiện tại; không quay về.
int exit(int) __attribute__((noreturn));
// Chờ một tiến trình con kết thúc và lấy mã kết thúc nếu được yêu cầu.
int wait(int *);
// Tạo một kênh pipe và ghi hai mô tả tệp đầu ra vào mảng fd.
int pipe(int *);
// Ghi tối đa n byte từ bộ đệm vào mô tả tệp fd.
int write(int, const void *, int);
// Đọc tối đa n byte từ mô tả tệp fd vào bộ đệm.
int read(int, void *, int);
// Đóng mô tả tệp fd.
int close(int);
// Gửi tín hiệu kết thúc tới tiến trình có PID pid.
int kill(int);
// Thay thế tiến trình hiện tại bằng chương trình path với các đối số argv.
int exec(const char *, char **);
// Mở tệp theo đường dẫn với các cờ truy cập được chỉ định.
int open(const char *, int);
// Tạo một nút thiết bị với số major và minor.
int mknod(const char *, short, short);
// Xóa tên tệp khỏi thư mục.
int unlink(const char *);
// Lấy thông tin của tệp từ mô tả tệp fd.
int fstat(int fd, struct stat *);
// Tạo một tên tệp mới trỏ tới cùng inode với tệp đã có.
int link(const char *, const char *);
// Tạo thư mục.
int mkdir(const char *);
// Chuyển thư mục làm việc hiện tại.
int chdir(const char *);
// Tạo một mô tả tệp mới trỏ tới cùng tệp với fd.
int dup(int);
// Trả về PID của tiến trình hiện tại.
int getpid(void);
// Gọi sbrk mức thấp; delta điều chỉnh kích thước bộ nhớ, mode chọn cách cấp phát.
char *sys_sbrk(int, int);
// Dừng tiến trình cho đến khi có ít nhất n nhịp đồng hồ (tick).
int pause(int);
// Trả về thời gian hệ thống tính bằng nhịp đồng hồ (tick).
int uptime(void);
// Đồng bộ các thay đổi tệp đang chờ ghi xuống hệ thống lưu trữ.
int sync(void);

// Các hàm tiện ích được cài đặt trong ulib.c.
// Lấy thông tin tệp theo đường dẫn.
int stat(const char *, struct stat *);
// Sao chép chuỗi nguồn, bao gồm ký tự kết thúc null.
char *strcpy(char *, const char *);
// Sao chép n byte, hỗ trợ cả trường hợp hai vùng nhớ chồng lấn.
void *memmove(void *, const void *, int);
// Tìm ký tự c đầu tiên trong chuỗi.
char *strchr(const char *, char c);
// So sánh hai chuỗi; trả về 0 nếu chúng bằng nhau.
int strcmp(const char *, const char *);
// Đọc một dòng tối đa max-1 ký tự vào bộ đệm.
char *gets(char *, int max);
// Trả về độ dài chuỗi, không tính ký tự kết thúc null.
uint strlen(const char *);
// Gán n byte đầu của vùng nhớ thành giá trị c.
void *memset(void *, int, uint);
// Chuyển chuỗi số thành số nguyên.
int atoi(const char *);
// So sánh n byte của hai vùng nhớ.
int memcmp(const void *, const void *, uint);
// Sao chép n byte giữa hai vùng nhớ không chồng lấn.
void *memcpy(void *, const void *, uint);
// Mở rộng bộ nhớ heap theo cách cấp phát eager (ngay lập tức).
char *sbrk(int);
// Mở rộng bộ nhớ heap theo cách cấp phát lazy (trì hoãn).
char *sbrklazy(int);

// Các hàm in định dạng được cài đặt trong printf.c.
// In ra mô tả tệp fd theo định dạng printf.
void fprintf(int, const char *, ...) __attribute__((format(printf, 2, 3)));
// In ra console theo định dạng printf.
void printf(const char *, ...) __attribute__((format(printf, 1, 2)));

// Các hàm quản lý bộ nhớ heap được cài đặt trong umalloc.c.
// Cấp phát một khối bộ nhớ có kích thước n byte.
void *malloc(uint);
// Giải phóng khối bộ nhớ đã cấp phát.
void free(void *);
