# Kết quả kiểm thử Thành viên 1

## Kiểm tra tích hợp mới nhất: 20/10/2026

Đã merge `origin/util` tại `1c1c51e` vào nhánh riêng `feature/m1-util-memdump-shell`.
Find/Exec đến từ thành viên 2; không sửa logic của bài đó.

| Kiểm tra trên bản tích hợp | Kết quả |
| --- | --- |
| Memdump với trang nhớ bảo vệ | 15/15 ca, OK |
| Shell/console tương tác thực trong QEMU | 12/12 test, OK |
| Grader Memdump chính thức | 3/3 nhóm, OK |
| Toàn bộ grade-lab-util | 95/156; 10 nhóm OK, 9 nhóm FAIL |
| clang-format 23.1.3, toàn bộ C/header | OK |
| git diff --check | OK |

Chín nhóm FAIL còn lại thuộc sleep (4), sixfive (4) và time.txt (1), ngoài phân công này.
Find/Exec đạt cả 7 nhóm. `exec via sh script` vẫn in cảnh báo Timeout từ monitor cũ, nhưng cuối cùng
được grader đánh dấu OK vì nội dung đúng. Không sửa grader.

CI Linux của commit cũ `094b77f` dừng tại `make fmt && git diff --exit-code`.
Kiểm tra cục bộ xác định chỉ dư dòng trống ở `kernel/param.h` và `kernel/riscv.h`;
đã chuẩn hóa hai file này, xác nhận không thay đổi token C. Đây là thay đổi format bổ sung.
CI của commit mới cần xem trực tiếp trên PR; kết quả cục bộ không thay thế trạng thái CI.

Log tích hợp: `evidence/integration-grade.log`, `evidence/integration-grades.json`.
Log Memdump và Shell trong evidence/ được cập nhật từ lần chạy này.
Patch `Release/util.patch` đã kiểm tra áp dụng trên `util` tại `1c1c51e` và đối chiếu từng file nguồn.
Báo cáo PDF giữ snapshot ngày 19/10/2026; phần dưới là dữ liệu lịch sử trước khi tích hợp Find/Exec.

## Snapshot trước khi tích hợp Find/Exec

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

Format: các file C/header đã sửa qua clang-format 23.1.3 và git diff --check. Kiểm tra format toàn repo còn báo
ở kernel/param.h và kernel/riscv.h vốn không thuộc thay đổi này; không chỉnh hai file của base. clang-format 18
đi kèm Ubuntu 24.04 không đọc được giá trị ReflowComments: Never trong cấu hình gốc, nên dùng bản mới hơn để kiểm tra.

## Review và tái hiện lỗi trước khi sửa

- Memdump trống: 15 ca guard-page đều thiếu output; sau sửa đều OK.
- Shell gốc: test script có prompt thừa; ec + Tab không hoàn thành trước Enter; !! bị exec như tên chương trình.
- Đã tái hiện và sửa raw-mode cleanup công bố sớm cooked input khi background exit.
- Review độc lập phát hiện Tab có thể đọc /console; test prefix rỗng tái hiện việc chặn rồi qua sau khi lọc T_FILE.
- Review cuối xác nhận các vấn đề quan trọng đã được sửa; kết quả runtime do người triển khai chạy và lưu riêng.

Log đầy đủ và transcript đại diện nằm trong evidence/. Log khi chạy lại nằm trong test-results/.
