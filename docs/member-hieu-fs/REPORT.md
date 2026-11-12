# Bàn giao kỹ thuật - Trung Hiếu - Đồ án 2

## 1. Phạm vi và nền code

Thực hiện mục 4.2.1 (file lớn) và 4.2.2 (symbolic link) theo Plan_DoAn2.md.
Theo chỉ đạo bổ sung của người dùng, tách nhánh `trung-hieu/largefile-symlink` từ commit
`43088aa420ae619d50f0595c910650659bef4874` đang có trong repo đồ án 1, thay vì chờ `origin/fs`.
Không thực hiện các bài page table. Giữ lại Shell/console, Find/Exec và công việc đã có.

Nhánh chọn `LAB=fs` trong `conf/lab.mk`; vì vậy có `FSSIZE=200000` theo cấu hình có sẵn.
Đổi bố cục inode yêu cầu ảnh đĩa mới. Không dùng lại fs.img/fs.img.bk của đồ án 1.
Leader cần thống nhất base này khi xuất patch cuối; không so sánh tùy ý với một nhánh fs khác.

## 2. File lớn: inode giữ nguyên 64 byte

`kernel/fs.h` và `kernel/file.h` cùng dùng 13 địa chỉ: 11 trực tiếp, 1 gián tiếp đơn và 1 gián tiếp kép.

```c
#define NDIRECT   11
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE   (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)
```

Với BSIZE=1024 và uint=4 byte, một block chỉ mục chứa 256 địa chỉ. File tối đa
65.803 block dữ liệu = 67.382.272 byte (khoảng 64,3 MiB), không tính block chỉ mục.
`addrs[NDIRECT + 2]` giữ đủ 13 ô trong cả inode trên RAM và inode trên đĩa.

### Điểm sửa quyết định trong bmap()

Sau khi trừ vùng trực tiếp và gián tiếp đơn, chia block logic còn lại thành hai chỉ số:

```c
uint outer = bn / NINDIRECT;
uint inner = bn % NINDIRECT;
```

`addrs[12] -> bảng ngoài[outer] -> bảng trong[inner] -> block dữ liệu`.
Cấp phát từng tầng khi cần; kiểm tra balloc() trả 0, nhả buffer ở mọi đường trả về.
Khi sửa địa chỉ trong block chỉ mục phải log_write(); inode được writei() cập nhật theo cơ chế gốc.
Không cấp phát sẵn toàn bộ cây.

### Thu hồi và dung lượng journal

`itrunc()` giải phóng dữ liệu trước, rồi bảng trong, cuối cùng bảng ngoài; xóa các ô địa chỉ inode,
đặt size=0 và iupdate(). Nhờ đó cả unlink và open với O_TRUNC thu hồi đầy đủ các tầng.

Một file phân mảnh trên disk 200.000 block có thể chạm cả 25 block bitmap và block inode trong một transaction.
Reservation cũ MAXOPBLOCKS=10 không đủ cho trường hợp này khi có thao tác đồng thời.
Trong LAB_FS đặt MAXOPBLOCKS=32, kéo LOGBLOCKS và NBUF lên 96 theo công thức có sẵn.
`kernel/file.c` vẫn giới hạn mỗi đợt write tối đa 3 block, thay vì tự tăng theo reservation mới,
để chừa chỗ cho metadata gián tiếp và bitmap. Chi phí: journal và cache lớn hơn; đổi lại không thêm
quy trình truncate nhiều transaction vốn phức tạp về crash recovery.

`mkfs/mkfs.c` cũng hiểu gián tiếp kép và ghi được nhiều block bitmap. Điều này tránh vượt mảng chỉ mục
khi đưa file lớn vào image, không chỉ khi tạo file từ trong xv6. Test host dùng file 9.000 block để đi qua
cả giới hạn gián tiếp đơn và giới hạn bitmap 8.192 block.

## 3. Symlink: ABI, tạo và mở

| Thành phần | Giá trị / file |
| --- | --- |
| Loại inode | T_SYMLINK=4, kernel/stat.h |
| Cờ open | O_NOFOLLOW=0x800, kernel/fcntl.h; không trùng cờ cũ |
| Syscall | SYS_symlink=24; giữ consolemode=23 của đồ án 1 |
| Giao diện user | int symlink(const char *target, const char *path) |
| Định tuyến | user.h, usys.pl, syscall.h, syscall.c, sysfile.c |

`sys_symlink()` nhận hai chuỗi bằng argstr(), từ chối chuỗi rỗng, quá MAXPATH và địa chỉ user sai.
Lưu target cùng NUL trong dữ liệu inode. Không tra đích khi tạo, nên dangling link được phép tồn tại.
Trả 0 khi thành công, -1 khi lỗi.

### Không công bố link chưa có dữ liệu

Helper create() nhận thêm target. Với T_SYMLINK, ghi target trước dirlink(), trong khi vẫn giữ khóa parent:

```c
  if (type == T_SYMLINK) {
    int len = strlen(target) + 1;
    if (writei(ip, 0, (uint64)target, 0, len) != len)
      goto fail;
  }
```

Nếu hết chỗ, nhánh fail hạ nlink và iunlockput() để thu hồi inode chưa công bố. Tiến trình khác chỉ thấy
link khi nội dung đã sẵn sàng. Cách này dùng lại cơ chế tạo inode và transaction hiện có, tránh tạo link
trống rồi phải gỡ directory entry khi write thất bại. Các caller mkdir/mknod/open được cập nhật tham số.

### open() lần theo link, tối đa 10 lần

```c
while (ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)) {
```

Mỗi bước kiểm tra kích thước và NUL của payload, đọc target vào buffer kernel, nhả khóa/ref inode cũ rồi
namei()/ilock() đích. Không giữ đồng thời hai khóa inode khi theo chuỗi. Quá 10 link, đích không tồn tại,
payload sai hoặc vòng lặp đều trả -1 và nhả tài nguyên.

O_NOFOLLOW mở chính inode link; đọc được chuỗi target. O_CREATE trên link có sẵn không thay thế link;
link đứt vẫn báo lỗi khi mở theo. O_TRUNC tác động file đích nếu follow, không truncate inode symlink
khi NOFOLLOW. link()/unlink() giữ cơ chế gốc, thao tác chính link, không đụng nội dung đích.

Giới hạn có chủ đích: chỉ open() follow symlink cuối đường dẫn; link tới thư mục không được hỗ trợ.
Target tương đối được giải từ cwd của tiến trình theo cách dùng namei() trong lab, không cam kết toàn bộ
ngữ nghĩa symlink POSIX (đặc biệt là giải target tương đối từ thư mục chứa link).

## 4. Kiểm thử và demo

Chi tiết kết quả và SHA xem TEST_RESULTS.md. Test bổ sung được tự viết cho repo hiện tại; không gọi chúng
là grader chính thức của thầy. Không sửa grader hoặc thay kỳ vọng của usertests gốc.

| Test | Mục đích |
| --- | --- |
| bigfile | Ghi 65.803 block có mẫu riêng; từ chối block tiếp; đọc lại toàn bộ |
| Runner largefile | Reboot, kiểm dữ liệu; full-size truncate/unlink; bitmap trở về ban đầu; tạo lại file gián tiếp kép |
| symlinktest | Link thường/đứt, 10/11 tầng, cycle, NOFOLLOW, O_CREATE/O_TRUNC, hardlink/unlink, input sai, cạn fd, tạo đồng thời |
| Fixture fsedge | Disk đầy; rollback symlink; file phân mảnh qua 25 bitmap block được truncate khi có writer khác |
| Fixture double ENOSPC | Còn 0/1/2 block trống: lỗi cấp phát root/bảng trong/dữ liệu; truncate thu hồi các bảng đã cấp phát dở |
| test_mkfs_large.py | File 9.000 block trong image, nội dung và địa chỉ mọi block, bitmap nhiều trang |
| usertests -q | Test hệ thống có sẵn, trên cùng bản tích hợp |

Trong Ubuntu/WSL, tại repo:

```sh
make clean
python3 tests/run_project2.py --suite all
```

Runner build LAB=fs, dùng image tạm và lưu log trong test-results/project2/ (đã ignore).
Test dữ liệu dùng 1 CPU để giảm chi phí QEMU; symlink concurrency và fixture phân mảnh dùng 3 CPU.
Fixture phân mảnh chỉnh bitmap chỉ trong image tạm, không dùng làm ảnh demo hoặc nộp bài.
Runner dùng cache=unsafe để test nhanh; kiểm tra reboot là khởi động lại QEMU có chủ đích, không phải chứng minh an toàn mất điện.

Demo thủ công:

```sh
make qemu
```

Trong xv6: `bigfile`, `symlinktest`, `usertests -q`. Các test tạo/xóa dữ liệu của riêng chúng.
Thoát QEMU bằng Ctrl-A rồi X. Không chuyển LAB rồi dùng lại file .o/image cũ; chạy make clean trước.

## 5. File thay đổi và điểm leader cần kiểm tra

- File lớn: fs.h, file.h, fs.c; mkfs.c; param.h và file.c cho reservation; conf/lab.mk chọn bài fs.
- Symlink: stat.h, fcntl.h, syscall.h/c, sysfile.c; user.h, usys.pl; Makefile thêm symlinktest trong LAB=fs.
- Test: user/bigfile.c, user/symlinktest.c; tests/run_project2.py, test_mkfs_large.py, fsedge.c.
- Handoff: báo cáo này, kết quả test và kế hoạch triển khai; log, image, binary không commit vào code.

Khi ghép với phần pgtbl, không chép đè cấu hình LAB/fs image sang nhánh pgtbl. Kiểm tra syscall 24 có bị
nhánh khác dùng hay không. Tạo patch cuối từ đúng SHA gốc nhóm thống nhất; gói này có thêm thay đổi so với nền util.
Leader duyệt và merge; người thực hiện chỉ push branch cá nhân, không tự tạo/ghi lên submit/fs.
