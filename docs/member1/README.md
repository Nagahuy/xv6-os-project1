# Hướng dẫn phần Thành viên 1: Memdump và Shell

Repo: https://github.com/Nagahuy/xv6-os-project1

Base: `origin/util`, commit `8138d07`. Branch làm bài: `feature/m1-util-memdump-shell`.

## 1. Chạy trên máy hiện tại

Ubuntu WSL đã được cài GCC RISC-V, QEMU, GDB và pexpect. Mở Ubuntu rồi chạy:

```sh
cd /mnt/d/HieuLT/HDH/Proj1
make qemu
```

Hoặc từ PowerShell:

```powershell
wsl -d Ubuntu --exec bash -lc 'cd /mnt/d/HieuLT/HDH/Proj1 && make qemu'
```

Các lệnh `memdump`, `history`, `echo` chạy **bên trong xv6**, tại dấu `$`.
Các lệnh `make`, `python3`, `./grade-lab-util` chạy **bên ngoài QEMU**.
Thoát QEMU: nhấn Ctrl-A, thả ra, rồi nhấn X.

`make qemu` tạo filesystem mới. Dùng `make qemu-fs` để giữ filesystem lần trước.

Trên Linux mới, các gói tương ứng:

```sh
sudo apt-get install build-essential gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu \
  qemu-system-misc gdb-multiarch python3-pexpect bc
```

Repo hiện tại đã đặt `core.autocrlf=false`; giữ LF cho các script khi sửa bằng editor Windows.

## 2. Chạy kiểm thử

Trong Ubuntu, sau khi thoát mọi phiên QEMU đang mở:

```sh
cd /mnt/d/HieuLT/HDH/Proj1
bash tests/run-member1.sh
```

Lệnh này chạy:

1. 15 trường hợp Memdump trên Linux, với trang nhớ bảo vệ ở ngay sau vùng `data`.
2. 12 test Shell/console trong QEMU; gửi Tab và phím mũi tên trước Enter.
3. Ba nhóm test Memdump chính thức bằng `./grade-lab-util memdump`.

Log nằm trong `test-results/`. Các file này chỉ là kết quả chạy, được loại khỏi Git bằng `.git/info/exclude`.
Test Shell tạo image tạm có chương trình `consoleprobe` và các script kiểm tra; không sửa Makefile hoặc image demo.

Để chạy cả bộ chấm của nhóm:

```sh
bash tests/run-member1.sh --full-grade
```

Bộ chấm có các bài `sleep`, `sixfive`, `find`/`exec` và `time.txt` chưa có trong base. Vì vậy **toàn bộ grade chưa xanh**;
xem `TEST_RESULTS.md` để phân biệt các lỗi đó với phần Memdump/Shell. Không sửa grader để bỏ qua bài khác.

## 3. Kịch bản demo

### Memdump

```sh
memdump
echo deadc0de | memdump hhcccc
echo deadc0de | memdump p
echo a | memdump i
```

Lệnh cuối phải báo đúng `memdump: not enough data for 'i'`.
Địa chỉ hexa trong Example 4 được phép khác PDF đề bài.

### Shell 3a: script không có prompt thừa

```sh
echo echo SCRIPT_OK > m1script
sh < m1script
echo INTERACTIVE_OK
```

Shell đọc script chỉ in `SCRIPT_OK`; Shell bên ngoài vẫn hiện `$ ` sau khi script kết thúc.
`sh < findtest.sh` cũng không có prompt thừa, nhưng hiện sẽ báo lỗi `find` vì phần Find của thành viên khác chưa tích hợp.

### Shell 3b: Tab

- Gõ `ec`, nhấn Tab: hiện `echo`; gõ tiếp ` hello` và Enter.
- Gõ `cat REA`, nhấn Tab: hiện `cat README`; nhấn Enter.
- Gõ `gr`, nhấn Tab: in `grep grind`, rồi hiện lại `$ gr`.
- Nhấn Ctrl-U để xóa dòng đang nhập.
- Nhấn Tab trên dòng trống: liệt kê chương trình, không bị chặn bởi thiết bị `console`.
- Gõ `echo NOTFOUND_`, nhấn Tab: dòng giữ nguyên; có thể tiếp tục gõ.

### Shell 3c: History

Chạy `sh` để có một phiên Shell con với lịch sử mới, rồi nhập:

```sh
echo a
history
!1
!!
```

Sau `history`, phải có `1 echo a` và `2 history`.
`!1` và `!!` đều in `echo a` trước khi chạy lại.
Thử nhấn lên/xuống trước Enter; sửa lệnh bằng backspace rồi chạy.
Gõ `echo draft`, nhấn lên, rồi xuống: dòng nháp được khôi phục.
Nhấn Ctrl-D khi dòng trống để thoát Shell con.

History giữ 10 lệnh, đánh số tăng liên tục từ 1. Lệnh bị loại khỏi 10 mục gần nhất không còn gọi bằng `!n` được.
Lỗi gọi history không được lưu thành một entry; lệnh gọi lại được lưu dưới dạng đã mở rộng.

### Kiểm tra console bình thường

Chạy `cat`, gõ một dòng và dùng backspace trước Enter. Dòng phải được sửa đúng.
Nhấn Ctrl-D ở dòng trống để thoát `cat`. Shell phải tiếp tục nhận lệnh bình thường.

## 4. Cách code hoạt động

- `memdump()` kiểm tra số byte trước, dùng `memmove` vào biến có căn chỉnh rồi in. `S` in tối đa số byte còn lại.
- Shell dùng `fstat(0)` để xác định đầu vào tương tác; script/pipe không bật prompt hoặc raw mode.
- `consolemode(1)` bật nhận từng byte không echo cho PID sở hữu. Shell tự echo/chỉnh dòng và xử lý ESC.
- `consolemode(0)` trả console về bình thường trước khi thực thi lệnh. Kernel tự giải phóng chế độ khi owner exit, bị kill hoặc exec thành công.
- Tab đọc directory entry có tên tối đa `DIRSIZ=14`; khi tìm chương trình, chỉ đọc ELF magic từ `T_FILE`, không đọc thiết bị.
- History là vòng đệm 10 phần tử; không lưu trên đĩa, không chia sẻ giữa các Shell.

## 5. Phạm vi thay đổi cần leader review

Người dùng đã đồng ý mở rộng kernel và syscall ngày 19/10/2026 và xác nhận sẽ báo leader.
`consolemode` dùng số syscall 23 **trên branch util**. Nhóm cần chú ý số hiệu nếu chủ động ghép code với branch syscall.

Code chính: `user/memdump.c`, `user/sh.c`.
Console: `kernel/console.c`, `kernel/defs.h`, `kernel/exec.c`, `kernel/proc.c`.
Định tuyến syscall: `kernel/syscall.h`, `kernel/syscall.c`, `kernel/sysproc.c`, `user/user.h`, `user/usys.pl`.

Makefile, grader, Find và các bài System calls không được thay đổi.
`.gitattributes` chỉ cho phép giữ nguyên whitespace trong patch/log bằng chứng; mã nguồn vẫn được kiểm tra whitespace bình thường.

Giới hạn: tối đa 127 ký tự/dòng; history chỉ tồn tại trong phiên hiện tại; không thêm job control, sửa ở giữa dòng bằng trái/phải,
hoặc PATH lookup. Khi `cd` sang thư mục con, gọi chương trình bằng đường dẫn như `/cat` theo cơ chế xv6 gốc.
Format `s` giả định địa chỉ chuỗi đầu vào hợp lệ; `len` chỉ giới hạn 8 byte chứa con trỏ, không xác thực vùng đích.

## 6. Gửi leader

- `REPORT.md` / `Report_Member1.pdf`: phần report để tổng hợp, chưa phải report đầy đủ cả nhóm.
- `TEST_RESULTS.md`: kết quả, các lỗi có sẵn và phạm vi xác minh.
- `VIVA.md`: câu hỏi tự vấn đáp.
- `PR.md`: mô tả PR.
- `Release/util.patch`: patch code riêng của Thành viên 1 so với base `origin/util`; chưa chứa bài của người khác.

Sau khi tích hợp các thành viên, leader cần tạo lại `util.patch` cho toàn nhóm, tổng hợp `Report.pdf` và `syscall.patch` theo đề.
