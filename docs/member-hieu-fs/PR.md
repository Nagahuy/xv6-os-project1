fs: support large files and symbolic links (Trung Hiếu)

Hoàn thành Plan_DoAn2.md mục 4.2.1 và 4.2.2 trên nhánh trung-hieu/largefile-symlink.
Theo chỉ đạo bổ sung, lấy nền util tại 43088aa thay vì origin/fs; cần giữ mốc này khi review/xuất patch.

- Giữ inode 64 byte / 13 địa chỉ: 11 trực tiếp + gián tiếp đơn + kép, tối đa 65.803 block.
- bmap cấp phát theo nhu cầu; itrunc thu hồi đủ dữ liệu và cả hai tầng chỉ mục.
- mkfs hỗ trợ cùng bố cục và bitmap nhiều block. LAB=fs dùng disk 200.000 block.
- Reservation journal 32 block/op, cache/log 96 block cho truncate phân mảnh; write batch vẫn tối đa 3 block.
- Syscall symlink=24 (consolemode=23 giữ nguyên), T_SYMLINK=4, O_NOFOLLOW=0x800.
- Ghi target trước khi công bố directory entry; open theo tối đa 10 link, xử lý lỗi và NOFOLLOW.
- Giữ cơ chế link/unlink trên chính link; target tương đối giải từ cwd, chỉ follow tại open, không hỗ trợ directory symlinks.

Hai commit code riêng: fa40ea4 (file lớn), a0c0425 (symlink). Báo cáo và kết quả cuối:
docs/member-hieu-fs/REPORT.md và TEST_RESULTS.md. Các test bổ sung do người thực hiện viết cho base này,
không phải bộ grader được thầy cung cấp. Giữ nguyên usertests và các grader hiện có.

Kiểm chứng: xem TEST_RESULTS.md để biết SHA đã test, lệnh, kết quả và giới hạn chính xác.
Chỉ leader merge vào branch tích hợp. Không đưa disk image, log/binary/patch/ZIP vào commit code.
