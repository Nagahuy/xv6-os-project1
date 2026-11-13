# Kết quả kiểm thử phần Trung Hiếu - Đồ án 2

Ngày thực hiện: 12/11/2026. Base: 43088aa420ae619d50f0595c910650659bef4874.
Branch: trung-hieu/largefile-symlink. Môi trường: Ubuntu WSL2, GCC RISC-V, QEMU, Python pexpect.

## Bằng chứng trước khi sửa

- bigfile trên kernel cũ dừng ở 268 block, báo FAIL thay vì 65.803 block.
- symlinktest gọi ABI dự kiến qua stub test tạm: unknown sys call 24, FAIL: create symlink.
- mkfs gốc ở 43088aa không đưa được payload 9.000 block vào ảnh: assert fbn < MAXFILE.
- Rà soát độc lập chỉ ra reservation journal cũ không bao phủ truncate phân mảnh qua 25 bitmap page.
  Fixture stress đã chạy được trên cả cấu hình cũ và mới; không khẳng định đã tái hiện panic bằng fixture này.
  Bản sửa reservation dựa trên giới hạn số block metadata tối đa và kiểm tra lại tính đồng thời.

## Kết quả hiện tại

Source kernel/user đã đối chiếu SHA-256 với commit `a0c04255b5b3c237ecbb794e4b6f2f5707f7030c` (17 file build liên quan).

| Kiểm tra | Kết quả |
| --- | --- |
| bigfile | wrote 65803 blocks; done; ok; dữ liệu mọi block đúng |
| Reboot / truncate / unlink / tái sử dụng | Đạt; bitmap trở về ban đầu; file đầy dùng thêm đúng 66.061 block gồm dữ liệu + chỉ mục |
| symlinktest | Cả 4 nhóm đạt, gồm test concurrent symlinks: ok; chạy 3 CPU |
| Phân mảnh / disk đầy | Đạt: truncate qua 25 bitmap page với 2 writer; rollback symlink không công bố entry lỗi |
| Cấp phát gián tiếp kép khi hết đĩa | Đạt 3 ca còn 0/1/2 block; bitmap xác nhận thu hồi cả index đã cấp phát dở |
| mkfs payload lớn | Đạt 9.000 block, kiểm mọi nội dung/địa chỉ và bitmap nhiều page |
| Shell cũ | 14/14 test QEMU đạt trên cấu hình LAB=fs |
| Memdump cũ | 15/15 ca guard-page đạt |
| clang-format / git diff --check | Đạt trên các file thay đổi |
| usertests -q | Đạt đủ 67 ca; ALL TESTS PASSED |

Build không có lỗi compiler. Cảnh báo linker RWX ở kernel/forktest có từ base; không sửa phần không liên quan.
Không suy kết quả local thành CI GitHub hoặc điểm chấm của thầy. Cần leader chạy lại test trên nhánh tích hợp.
Các dòng usertrap trong negative test của usertests là kết quả cố ý truy cập sai của process con;
bộ test kết thúc ALL TESTS PASSED, không có SOME TESTS FAILED.

## Lệnh đã chạy và kết quả chính

Trong bản sao Linux sạch với cùng source đã đối chiếu:

```sh
python3 tests/run_project2.py --suite all
# PASS largefile: 65803 indexed blocks, reboot, truncate, reuse, unlink; bitmap restored
# PASS symlinktest
# PASS usertests -q
# PASS fragmented truncate across 25 bitmap pages with concurrent writers, ENOSPC rollback

python3 tests/run_project2.py --suite fragmented
# PASS fragmented truncate across 25 bitmap pages with concurrent writers, ENOSPC rollback
# PASS double-indirect ENOSPC with 0/1/2 free blocks; partial indices reclaimed

python3 tests/test_mkfs_large.py
# PASS mkfs: 9000 blocks, all data and index addresses, multiple bitmap blocks

python3 tests/test_shell.py
# Ran 14 tests ... OK
python3 tests/test_memdump.py
# 15 subcases ... OK
```

Các lệnh Python trên chạy ở terminal Linux, không phải tại dấu nhắc xv6.
Runner điều khiển QEMU và chạy bigfile/symlinktest/usertests bên trong xv6.
Thay đổi cuối của runner đã đưa test mkfs và các ca ENOSPC bổ sung vào --suite all để lần chạy sau dùng một lệnh.
Trong lần xác minh hiện tại, hai kiểm tra bổ sung này được chạy riêng bên cạnh lượt --suite all đang chạy.

Commit file lớn: `fa40ea4`. Commit symlink và source tích hợp đã test: `a0c0425`.
Rà soát độc lập đã thực hiện; reservation journal được sửa sau phát hiện và xác minh lại toàn bộ suite.
Không còn phát hiện quan trọng chưa xử lý trong lần review này; không xem đây là bảo đảm không còn lỗi.

## Trạng thái bàn giao

Đã push nhánh cá nhân `trung-hieu/largefile-symlink`. Theo trả lời mới nhất của người dùng,
chưa mở PR; để leader tạo/chốt nhánh nhận. Nội dung PR chuẩn bị tại PR.md.
Chỉ commit source, test và báo cáo; không commit image, log, file build hoặc patch/ZIP.

## Vị trí log và cách chạy lại

Log không commit theo plan leader. Máy hiện tại: test-results/project2/final/ cho lần chạy cuối;
mkfs-before.log, mkfs-after.log, largefile-before.log, symlink-before.log trong test-results/project2/.
Nếu chạy trực tiếp runner tại repo thì log nằm ở test-results/project2/.

```sh
make clean
python3 tests/run_project2.py --suite all
```

Ở máy Windows hiện tại, build/test được chạy trong bản sao source dưới /tmp/xv6-hieu-project2-final của WSL,
để tránh ghi hàng trăm MB qua ổ Windows. Source kernel/user của bản sao được đối chiếu với working tree trước khi chốt.
Các image đều tạo mới; không dùng lại image của đồ án 1.

Không có bigfile/symlinktest/grade-lab-fs được cung cấp trên base này. Các chương trình bigfile và symlinktest
trong nhánh là test hành vi tự viết theo plan; usertests -q là bộ có sẵn, giữ nguyên.
