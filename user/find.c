#include "kernel/types.h"   // uint, ushort, v.v.
#include "kernel/stat.h"    // struct stat, T_DIR, T_FILE
#include "user/user.h"      // khai báo syscall: open, read, close, fstat, stat, strcmp...
#include "kernel/fs.h"      // struct dirent, DIRSIZ
#include "kernel/param.h"

// void find(char *path, char *name);
void run_cmd(char **cmd_argv, char *filename);
void find(char *path, char *target_name, char **cmd_argv);

int main(int argc, char *argv[]) {
    // 1. Kiểm tra cú pháp tối thiểu (Ví dụ: find . b)
    if (argc < 3) {
        fprintf(2, "Usage: find [path] [target_name] [-exec cmd_args...]\n");
        exit(1);
    }

    char *path = argv[1];
    char *target_name = argv[2];
    char **cmd_argv = 0; // Mặc định là không có -exec

    // 2. Tìm xem người dùng có gõ "-exec" ở tham số thứ 3 không
    if (argc > 3) {
        if (strcmp(argv[3], "-exec") == 0) {
            // Kiểm tra xem sau chữ "-exec" có lệnh nào không
            if (argc < 5) {
                fprintf(2, "find: missing command after -exec\n");
                exit(1);
            }
            // &argv[4] nghĩa là lấy mảng các tham số bắt đầu từ sau chữ "-exec" trở đi
            cmd_argv = &argv[4]; 
        } else {
            fprintf(2, "find: invalid argument %s\n", argv[3]);
            exit(1);
        }
    }

    // 3. Gọi hàm find phiên bản 3 tham số
    find(path, target_name, cmd_argv);
    exit(0);
}


// void find(char *path, char *name) {
//     int fd;
//     struct stat st;
    
//     if ((fd = open(path, 0)) < 0) {       // 0 = O_RDONLY
//         printf("find: cannot open %s\n", path);
//         return;
//     }
    
//     if (fstat(fd, &st) < 0) {
//         printf("find: cannot stat %s\n", path);
//         close(fd);
//         return;
//     }
//     char *p;
//     // Tùy theo type:
//     switch (st.type) {
//         case T_FILE:
//             // Kiểm tra tên file có khớp với name không
//             // Tìm ký tự '/' cuối cùng
//             for (p = path + strlen(path); p >= path && *p != '/'; p--);
//             p++;  // p giờ trỏ vào tên file

//             // So sánh tên với name
//             if (strcmp(p, name) == 0) {
//                 printf("%s\n", path);   // in đường dẫn đầy đủ
//             }
//             break;
//         case T_DIR:
//             // Duyệt thư mục đệ quy
//             // Trong nhánh T_DIR:
//             struct dirent de;
//             char buf[512];

//             // Copy path vào buf, thêm dấu '/'
//             strcpy(buf, path);
//             p = buf + strlen(buf);
//             *p++ = '/';               // buf = "path/", p trỏ vào chỗ sẽ ghi tên con

//             while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                
//                 if (de.inum == 0)     // Entry trống → bỏ qua
//                     continue;
                
//                 // Bỏ qua "." và ".." để tránh đệ quy vô tận
//                 if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
//                     continue;
                
//                 // Ghép tên vào buf: buf = "path/filename"
//                 memmove(p, de.name, DIRSIZ);
//                 p[DIRSIZ] = '\0';     // đảm bảo kết thúc chuỗi!
                
//                 struct stat st2;
//                 if (stat(buf, &st2) < 0) {
//                     printf("find: cannot stat %s\n", buf);
//                     continue;
//                 }
                
//                 if (st2.type == T_DIR) {
//                     find(buf, name);          // Đệ quy vào thư mục con
//                 } else if (st2.type == T_FILE) {
//                     if (strcmp(de.name, name) == 0)
//                         printf("%s\n", buf);  // Tên khớp → in ra
//                 }
//             }
//             break;
//     }
//     close(fd);
// }

void run_cmd(char **cmd_argv, char *filename) {
    // 1. Tạo một mảng mới để chứa toàn bộ lệnh + filename + kết thúc bằng 0 (NULL)
    char *new_argv[MAXARG];
    int i = 0;

    // 2. Sao chép các từ trong cmd_argv sang new_argv
    // Cần tìm hiểu chỗ này , tại sao lại thêm i < MAXARG - 2  ,nếu không thêm thì sao?,có liên quan gì đến (shallow copy) không?
    
    while (cmd_argv[i] != 0  && i < MAXARG - 2) {
        new_argv[i] = cmd_argv[i];
        i++;
    }

    // 3. Nối tên file tìm được vào cuối danh sách đối số
    new_argv[i] = filename;
    new_argv[i + 1] = 0; // Luôn kết thúc mảng con trỏ bằng 0 (NULL) để exec biết điểm dừng

    // 4. fork() để tạo tiến trình con
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "find: fork failed\n");
        return;
    }

    if (pid == 0) {
        // --- ĐÂY LÀ TIẾN TRÌNH CON ---
        // Gọi exec với chương trình là new_argv[0] và danh sách tham số new_argv
        exec(new_argv[0], new_argv);

        // Nếu exec thành công, nó sẽ ghi đè toàn bộ tiến trình này bằng lệnh mới.
        // Dòng code dưới đây CHỈ CHẠY khi exec THẤT BẠI:
        fprintf(2, "find: exec %s failed\n", new_argv[0]);
        exit(1);
    } else {
        // --- ĐÂY LÀ TIẾN TRÌNH CHA ---
        // Cha phải chờ con chạy xong rồi mới tiếp tục tìm file kế tiếp
        wait(0);
    }
}

void find(char *path, char *target_name, char **cmd_argv){
    int fd;
    struct stat st;
    
    if ((fd = open(path, 0)) < 0) {       // 0 = O_RDONLY
        printf("find: cannot open %s\n", path);
        return;
    }
    
    if (fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    char *p;
    // Tùy theo type:
    switch (st.type) {
        case T_FILE:
            // Kiểm tra tên file có khớp với name không
            // Tìm ký tự '/' cuối cùng
            for (p = path + strlen(path); p >= path && *p != '/'; p--);
            p++;  // p giờ trỏ vào tên file

            // So sánh tên với name
            if (strcmp(p, target_name) == 0) {
                // Nếu không có -exec, in ra đường dẫn
                if(cmd_argv == 0){
                    printf("%s\n", path);   
                }
                else{
                    // -exec :gọi hàm run_cmd 
                    run_cmd(cmd_argv, path);
                }
            }
            break;
        case T_DIR:
            // Duyệt thư mục đệ quy
            // Trong nhánh T_DIR:
            struct dirent de;
            char buf[512];

            // Copy path vào buf, thêm dấu '/'
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';               // buf = "path/", p trỏ vào chỗ sẽ ghi tên con

            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                
                if (de.inum == 0)     // Entry trống → bỏ qua
                    continue;
                
                // Bỏ qua "." và ".." để tránh đệ quy vô tận
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
                
                // Ghép tên vào buf: buf = "path/filename"
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = '\0';     // đảm bảo kết thúc chuỗi!
                
                struct stat st2;
                if (stat(buf, &st2) < 0) {
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                if (strcmp(de.name, target_name) == 0) {
                    if (cmd_argv == 0) {
                        printf("%s\n", buf);
                    } else {
                        run_cmd(cmd_argv, buf);
                    }
                }
                if (st2.type == T_DIR) {
                    find(buf, target_name, cmd_argv);          // Đệ quy vào thư mục con
                } 
            }
            break;
    }
    close(fd);
}