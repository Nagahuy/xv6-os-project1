#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

// void find(char *path, char *name);
void run_cmd(char **cmd_argv, char *filename);
void find(char *path, char *target_name, char **cmd_argv);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(2, "Usage: find [path] [target_name] [-exec cmd_args...]\n");
        exit(1);
    }

    char *path = argv[1];
    char *target_name = argv[2];
    char **cmd_argv = 0; 

    if (argc > 3) {
        if (strcmp(argv[3], "-exec") == 0) {
            if (argc < 5) {
                fprintf(2, "find: missing command after -exec\n");
                exit(1);
            }
            
            cmd_argv = &argv[4]; 
        } else {
            fprintf(2, "find: invalid argument %s\n", argv[3]);
            exit(1);
        }
    }

    find(path, target_name, cmd_argv);
    exit(0);
}

// void find(char *path, char *name) {
//     int fd;
//     struct stat st;
    
//     if ((fd = open(path, 0)) < 0) {       
//         printf("find: cannot open %s\n", path);
//         return;
//     }
    
//     if (fstat(fd, &st) < 0) {
//         printf("find: cannot stat %s\n", path);
//         close(fd);
//         return;
//     }
//     char *p;
//     switch (st.type) {
//         case T_FILE:
//             for (p = path + strlen(path); p >= path && *p != '/'; p--);
//             p++;  

//             if (strcmp(p, name) == 0) {
//                 printf("%s\n", path);   // in đường dẫn đầy đủ
//             }
//             break;
//         case T_DIR:
//             struct dirent de;
//             char buf[512];

//             strcpy(buf, path);
//             p = buf + strlen(buf);
//             *p++ = '/';              

//             while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                
//                 if (de.inum == 0)    
//                     continue;
                
             
//                 if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
//                     continue;
                
                
//                 memmove(p, de.name, DIRSIZ);
//                 p[DIRSIZ] = '\0';     
                
//                 struct stat st2;
//                 if (stat(buf, &st2) < 0) {
//                     printf("find: cannot stat %s\n", buf);
//                     continue;
//                 }
                
//                 if (st2.type == T_DIR) {
//                     find(buf, name);          
//                 } else if (st2.type == T_FILE) {
//                     if (strcmp(de.name, name) == 0)
//                         printf("%s\n", buf);  
//                 }
//             }
//             break;
//     }
//     close(fd);
// }


void run_cmd(char **cmd_argv, char *filename) {
    char *new_argv[MAXARG];
    int i = 0;

    while (cmd_argv[i] != 0  && i < MAXARG - 2) {
        new_argv[i] = cmd_argv[i];
        i++;
    }

    new_argv[i] = filename;
    new_argv[i + 1] = 0;

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "find: fork failed\n");
        return;
    }

    if (pid == 0) {
        exec(new_argv[0], new_argv);

        fprintf(2, "find: exec %s failed\n", new_argv[0]);
        exit(1);
    } else {
        wait(0);
    }
}

void find(char *path, char *target_name, char **cmd_argv){
    int fd;
    struct stat st;
    
    if ((fd = open(path, 0)) < 0) {
        printf("find: cannot open %s\n", path);
        return;
    }
    
    if (fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    char *p;
    switch (st.type) {
        case T_FILE:
            for (p = path + strlen(path); p >= path && *p != '/'; p--);
            p++;

            if (strcmp(p, target_name) == 0) {
                if(cmd_argv == 0){
                    printf("%s\n", path);
                }
                else{
                    run_cmd(cmd_argv, path);
                }
            }
            break;
        case T_DIR:
            struct dirent de;
            char buf[512];

            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';

            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                
                if (de.inum == 0)
                    continue;
                
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
                
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = '\0';
                
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
                    find(buf, target_name, cmd_argv);
                } 
            }
            break;
    }
    close(fd);
}

