# Báo cáo Thành viên 1
Memdump và cải tiến Shell - xv6 Utilities

Ngày thực hiện: 19/10/2026. Người thực hiện: Thành viên 1 theo phân công nhóm.
Nguồn: OS_project1.pdf, trang 2-3 và 5-6; OS.txt; yêu cầu phối hợp của leader.
Base: origin/util (8138d07). Branch: feature/m1-util-memdump-shell.

## 1. Memdump

Hoàn thiện memdump(fmt, data, len) trong user/memdump.c. Duyệt từng ký tự format, xác định kích thước cần đọc:
i=4, p=8, h=2, c=1, s=8 byte. Kiểm tra đủ dữ liệu trước khi copy vào biến tạm đúng kiểu và in kết quả.
Copy bằng memmove giúp xử lý cả địa chỉ không thẳng hàng sau một format c. Sau mỗi giá trị, tăng con trỏ và giảm len.

Khi thiếu dữ liệu, in đúng "memdump: not enough data for 'X'" rồi dừng. Format S in từng ký tự đến NUL hoặc
hết dữ liệu hợp lệ và kết thúc xử lý phần còn lại. Format s đọc con trỏ rồi in chuỗi đích; p dùng số hexa 64-bit.
Giữ nguyên main và các ví dụ có sẵn.

## 2. Shell 3a, 3b, 3c

3a: dùng fstat(0) để phân biệt đầu vào console với file/pipe. Chỉ phiên tương tác in prompt. Đọc script theo dòng;
dòng cuối không cần newline, dòng quá dài bị bỏ nguyên dòng để không chạy nhầm phần đuôi.

3b: tìm token đang nhập. Token đầu tìm chương trình ELF trong thư mục /; token sau tìm tên file ở cwd,
có hỗ trợ prefix đường dẫn. Chỉ một match thì thêm hậu tố; nhiều match thì in trên một dòng rồi dựng lại prompt và
dòng đang nhập; không match thì giữ nguyên. Copy directory entry vào buffer có NUL, xử lý được tên dài DIRSIZ=14.
Chỉ đọc ELF magic từ file thường để không chặn trên thiết bị console.

3c: dùng vòng đệm 10 lệnh. Số thứ tự tăng từ 1, không tái sử dụng khi entry cũ bị loại. Lưu cả history;
lệnh !! và !n được mở rộng, in ra trước khi chạy và lưu dưới dạng đã mở rộng. Báo lỗi khi chưa có lịch sử,
số không hợp lệ hoặc entry đã bị loại. Phím lên/xuống duyệt ngay trước Enter; đi xuống cuối khôi phục dòng nháp.

## 3. Nhập phím tức thời và bảo toàn console

Console gốc chỉ công bố dữ liệu theo dòng. Thêm consolemode(raw), syscall số 23 trên branch util:
Shell bật raw=1 khi nhập, nhận từng byte không echo và tự xử lý Tab, ESC, backspace, Ctrl-U, Ctrl-D;
tắt raw=0 trước khi xử lý lệnh. Raw mode có PID sở hữu, không bị process khác tắt hoặc chiếm.
Kernel giải phóng mode khi owner exit, bị kill hoặc exec thành công. Yêu cầu mode vốn đã hoạt động là no-op,
không công bố sớm dòng cooked đang nhập.

---

## 4. File và hàm thay đổi

user/memdump.c: memdump(). user/sh.c: getcmd(), main(), completion, mở rộng và lưu/duyệt history.
kernel/console.c: consolemode(), consoleintr(), consoleread(); kernel/defs.h: khai báo giao diện kernel.
kernel/proc.c và kernel/exec.c: thu hồi raw mode trong kexit()/kexec().
kernel/syscall.h, kernel/syscall.c, kernel/sysproc.c, user/user.h, user/usys.pl: số hiệu, stub và định tuyến syscall.
Không sửa Makefile, grader hoặc bài của thành viên khác.

## 5. Kiểm thử và kết quả

Môi trường: Ubuntu 24.04 trên WSL2; GCC RISC-V 13.3.0; QEMU 8.2.2; Python 3.12.

| Kiểm tra | Kết quả | Bằng chứng |
| --- | --- | --- |
| Memdump trên Linux | 15/15 ca qua | test-results/memdump-guard.log |
| Shell/console trong QEMU | 12/12 test qua | test-results/shell-after.log |
| Grader Memdump | 3/3 nhóm OK | test-results/memdump-grade.log |

Test Memdump dùng trang nhớ bị cấm ngay sau data để phát hiện đọc quá len, bao gồm S không có NUL,
s chứa con trỏ hợp lệ, thiếu 1 byte, đọc vừa đủ, số âm và địa chỉ không thẳng hàng.
Test Shell gửi Tab/mũi tên trước Enter, kiểm tra script/pipe/EOF, history quá 10 entry, dòng nháp,
tên file 14 ký tự, raw-mode ownership/exit/kill/exec và cooked input khi tiến trình nền kết thúc.

Đã chạy toàn bộ make grade và đối chiếu với kết quả trước thay đổi Shell. Các lỗi có sẵn nằm ở sleep,
sixfive, find/exec và time.txt, ngoài phân công Thành viên 1; danh sách test đạt/rớt không đổi, chi tiết trong TEST_RESULTS.md.
Test script của grader còn chờ timeout dù DONE đã được in do cơ chế theo dõi echo/prompt cũ; không sửa grader.
Không dùng điểm grader tổng làm điểm Shell: bài Shell được xác minh qua tương tác QEMU và vẫn cần demo khi vấn đáp.

## 6. Giới hạn và bàn giao

Dòng nhập tối đa 127 ký tự. History chỉ lưu trong RAM của một phiên Shell. Không bổ sung PATH lookup,
job control hoặc phím trái/phải để sửa giữa dòng. Khi ở thư mục con, dùng đường dẫn như /cat theo xv6 gốc.
Format s giả định chuỗi đích hợp lệ; len bảo vệ vùng chứa con trỏ, không xác thực toàn bộ chuỗi ở địa chỉ đích.

Bàn giao branch, patch riêng Release/util.patch, log test, README demo và VIVA.md. Report này chỉ là phần của Thành viên 1;
leader cần ghép bài còn lại, tên/MSSV, bảng đóng góp và patch toàn nhóm trước khi nộp Moodle.

| Bài | Điểm tối đa | Tự chấm đề xuất |
| --- | --- | --- |
| Memdump | 1,5 | 1,5 |
| Shell 3a/3b/3c | 1,5 | 1,5 |

Điểm đề xuất dựa trên chức năng và kiểm thử, chưa phải điểm giảng viên; mỗi bài gồm 60% code/test/report và 40% vấn đáp/demo.
