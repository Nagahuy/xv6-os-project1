# Kết quả kiểm thử Thành viên 1

Ngày: 19/10/2026. Base: origin/util, 8138d07.
Môi trường: Ubuntu 24.04 WSL2, RISC-V GCC 13.3.0, QEMU 8.2.2, Python 3.12.3.

## Kết quả phần được giao

| Kiểm tra | Kết quả |
| --- | --- |
| python3 tests/test_memdump.py | 15/15 trường hợp, OK |
| python3 tests/test_shell.py | 12/12 test, OK |
| ./grade-lab-util memdump | 3/3 nhóm, OK |
| make grade | 25/156 điểm nội bộ grader; 3 nhóm OK, 16 nhóm FAIL |

25/156 là điểm nội bộ của grader đang có cả bài cũ; không phải thang điểm 10/12 của PDF.
Phần Shell không có test chấm tự động chính thức; các test QEMU bổ sung hỗ trợ demo, không thay thế vấn đáp.

### Memdump

- Format hỗn hợp icS và hhcccc; p 64-bit; s chứa con trỏ chuỗi hợp lệ.
- S không có NUL và vùng kết thúc ngay trước trang bị cấm đọc; S gặp NUL sớm; S với len=0.
- i/p/h/c/s thiếu đúng một byte hoặc hết dữ liệu: báo lỗi, không đọc trang bảo vệ.
- Format ii chỉ đủ số đầu: in số đầu, báo lỗi ở số sau, không tiếp tục.
- Dữ liệu không thẳng hàng và số âm (-1, -32768).
- Ví dụ và các test bounds/usage chính thức đều OK.

### Shell và console

1. Script không in prompt thừa; Shell tương tác vẫn in prompt.
2. ec + Tab và cat REA + Tab phản hồi trước Enter.
3. gr + Tab liệt kê nhiều kết quả, không match giữ nguyên; Ctrl-U hoạt động.
4. history/!!/!n in và thực thi đúng lệnh; lưu entry đã mở rộng.
5. History rỗng, event không tồn tại, quá 10 lệnh, entry bị loại.
6. Up/Down phản hồi trước Enter, khôi phục dòng nháp, sửa lệnh bằng backspace.
7. Pipe và cat dùng chế độ console thông thường.
8. Raw owner không bị process con chiếm/tắt; thu hồi mode khi exit, kill và exec.
9. Tiến trình nền kết thúc không làm commit sớm dòng đang nhập cho cat; backspace vẫn sửa đúng.
10. Completion theo cwd, prefix thư mục, tên file đủ 14 ký tự không có NUL trên đĩa.
11. Script không có newline cuối, dòng quá dài bị bỏ nguyên dòng, đầu vào pipe, mở rộng history có khoảng trắng đầu.
12. Tab ở prefix rỗng không đọc thiết bị console và không chờ thêm phím.

Image test có fixture consoleprobe và script riêng, tạo trong thư mục tạm, chạy với snapshot.
Không thêm chương trình test vào Makefile hoặc filesystem demo của nhóm.

## Bộ chấm tổng và các lỗi còn có sẵn

Snapshot đối chiếu được chạy sau khi hoàn thiện Memdump nhưng **trước khi sửa Shell/kernel**.
baseline-grades.json và final-grades.json giống nhau về từng test và điểm: không có test trước đó OK chuyển sang FAIL.
Các test FAIL gồm:

- sleep, no arguments
- sleep, returns
- sleep, makes syscall
- sleep, zero and non-numeric args return
- sixfive_test
- sixfive_readme
- sixfive_all
- sixfive, stdin and missing file
- find, in current directory
- find, in sub-directory
- find, recursive
- exec
- exec, multiple args
- exec via sh script
- find, missing target, no duplicates, directory match
- time

Nguyên nhân: base chưa có các chương trình sleep/sixfive/find và chưa có time.txt. Không thực hiện bài của người khác
hoặc tự khai số giờ thay nhóm để làm xanh grader.

Riêng exec via sh script: sau khi bỏ prompt thừa, monitor shell_script(..., 'DONE') của gradelib.py có thể timeout.
Monitor dùng re.match vào buffer có thể bắt đầu bằng dòng echo lệnh thay vì DONE. Transcript vẫn cho thấy
`sh < findtest.sh` kết thúc, `echo DONE` đã chạy, in DONE và có prompt tiếp theo. Nó vẫn FAIL vì thiếu find;
thời gian chạy test cũng bị ảnh hưởng bởi monitor. Không thay grader để che lỗi. Khi nhóm tích hợp Find, cần trao đổi
về monitor nếu còn timeout và tiếp tục demo Shell trực tiếp theo đề.

Compiler không báo lỗi. Linker có cảnh báo LOAD segment RWX ở kernel/forktest, đã xuất hiện ngay trong build base.

## Review và tái hiện lỗi trước khi sửa

- Memdump trống: 15 ca guard-page đều thiếu output; sau sửa đều OK.
- Shell gốc: test script có prompt thừa; ec + Tab không hoàn thành trước Enter; !! bị exec như tên chương trình.
- Đã tái hiện và sửa raw-mode cleanup công bố sớm cooked input khi background exit.
- Review độc lập phát hiện Tab có thể đọc /console; test prefix rỗng tái hiện việc chặn rồi qua sau khi lọc T_FILE.
- Review cuối xác nhận các vấn đề quan trọng đã được sửa; kết quả runtime do người triển khai chạy và lưu riêng.

Log đầy đủ và transcript đại diện nằm trong evidence/. Log khi chạy lại nằm trong test-results/.
