# Tự vấn đáp: Memdump và Shell

1. **`len` ngăn đọc tràn như thế nào?**
   Mỗi format cố định có kích thước 1/2/4/8 byte. Hàm so sánh với số byte còn lại trước khi đọc, dừng khi thiếu;
   sau khi đọc mới tăng con trỏ và giảm độ dài. `S` kiểm tra độ dài trước từng lần đọc ký tự.

2. **Vì sao copy vào biến tạm thay vì ép `data` thành `int *`?**
   Một format trước đó có thể chỉ lấy 1 byte, khiến địa chỉ tiếp theo không căn chỉnh theo `int` hoặc `uint64`.
   `memmove` copy byte vào biến đúng kiểu/căn chỉnh mà không dựa vào truy cập lệch hàng.

3. **`s` và `S` khác nhau thế nào?**
   `s` lấy 8 byte chứa địa chỉ rồi in chuỗi tại địa chỉ đó. `S` in các ký tự ngay trong dữ liệu hiện tại,
   dừng khi gặp NUL hoặc hết `len`; `S` mô tả phần còn lại nên kết thúc xử lý format.

4. **Shell biết khi nào phải in prompt?**
   Gọi `fstat(0)` sau khi thiết lập các file descriptor chuẩn. Đầu vào là thiết bị console thì bật tương tác;
   đầu vào là file hoặc pipe thì đọc từng dòng mà không in prompt.

5. **Vì sao cần sửa kernel cho Tab và mũi tên?**
   Console gốc chỉ công bố đầu vào khi gặp newline, Ctrl-D hoặc buffer đầy. Tab phải phản hồi trước Enter.
   Raw mode đưa từng byte cho Shell ngay và để Shell tự echo/chỉnh dòng.

6. **Vì sao raw mode không làm hỏng `cat`?**
   Shell tắt mode trước khi chạy lệnh. PID khác không thể tắt hoặc chiếm mode của owner. Kernel giải phóng mode
   khi owner exit/kill/exec; yêu cầu tắt khi console vốn đã bình thường là no-op, không commit dòng đang nhập.

7. **Các buffer có kích thước nào và vì sao?**
   Dòng lệnh 128 byte: 127 ký tự và một NUL, cùng mức dung lượng console gốc. History có 10 entry theo đề.
   Tên directory entry có tối đa 14 ký tự, nên copy vào buffer 15 byte rồi thêm NUL.
   Buffer path của completion đủ chứa đường dẫn dòng lệnh cộng tên file và dấu phân cách.

8. **Tab phân biệt tên lệnh và tên file thế nào?**
   Token đầu tiên tìm chương trình ở `/`, các token sau tìm file ở cwd. Với chương trình, kiểm tra T_FILE và ELF magic;
   không đọc thiết bị console. Một match thì thêm hậu tố, nhiều match thì in danh sách và dựng lại prompt/dòng cũ.

9. **History đầy hoặc `!n` quá lớn thì sao?**
   Vòng đệm bỏ entry cũ nhất nhưng số lệnh vẫn tăng. `!n` chỉ truy tìm trong 10 entry hiện có.
   Phân tích số có kiểm tra ký tự và tràn số; số không tồn tại báo lỗi và không chạy lệnh.

10. **`!!` có gây lặp vô hạn không?**
    Không: Shell lấy nội dung entry trước, copy vào buffer lệnh, in ra rồi lưu chính nội dung đã mở rộng.
    Không lưu `!!` làm tham chiếu cần mở rộng đệ quy. Lịch sử được lưu trước khi parser ở process con sửa buffer.

11. **Vì sao grade tổng còn fail nhưng Memdump/Shell đã kiểm tra được?**
    Grader hiện có bài của thành viên khác và các test cũ của base. So sánh bảng kết quả trước/sau xác định regression;
    riêng Shell phải được chứng minh bằng tương tác QEMU, không được suy ra từ grade Memdump.
