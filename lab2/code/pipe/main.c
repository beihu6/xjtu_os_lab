/*管道通信实验程序残缺版 */ 
#include <unistd.h> 
#include <signal.h> 
#include <stdio.h> 
#include <stdlib.h>
int pid1,pid2; // 定义两个进程变量
int main(){
    int fd[2]; 
    char InPipe[5000]; // 定义读缓冲区
    char c1='1', c2='2'; 
    while(pipe(fd)==-1); // 创建管道
    FILE *fp;
    char buffer[1024];
    char command[100];
    sprintf(command, "ls -l /proc/%d/fd/", getpid());
    system(command);
    printf("\n");
    while((pid1 = fork( )) == -1); // 如果进程 1 创建不成功,则空循环
    if(pid1 == 0) { // 如果子进程 1 创建成功,pid1 为进程号
        close(fd[0]); // 关闭管道读端
        //lockf(fd[1],F_LOCK,0);// 锁定管道
        char *OutPipe="Child process 1 is sending message!";
        for(int i=1;i<=2000;i++){
            write(fd[1],&c1,1);
        } // 分 2000 次每次向管道写入字符’1’ 
        //sleep(3); // 等待读进程读出数据
        //lockf(fd[1],F_ULOCK,0); // 解除管道的锁定
        close(fd[1]);
        exit(0); // 结束进程 1 
    } 
    else { 
        while((pid2 = fork()) == -1); // 若进程 2 创建不成功,则空循环
        if(pid2 == 0) { 
            close(fd[0]); // 关闭管道读端
            char command[100];
            sprintf(command, "ls -l /proc/%d/fd/", getpid());
            system(command);
            printf("\n");
            //lockf(fd[1],1,0); 
            char *OutPipe="Child process 2 is sending message!";
            for(int i=1;i<=2000;i++){
                write(fd[1],&c2,1);
            }// 分 2000 次每次向管道写入字符’2’ 
            //sleep(3); 
            //lockf(fd[1],0,0); 
            close(fd[1]);
            exit(0); 
        } else { 
            close(fd[1]); // 关闭管道写端
            //wait(NULL); // 等待子进程 1 结束
            //wait(NULL); // 等待子进程 2 结束
            int total_read = 0;
            int bytes_read;
            while (total_read < 4000 && 
                (bytes_read = read(fd[0], InPipe + total_read, 4000 - total_read)) > 0) {
                total_read += bytes_read;
            }
            InPipe[total_read] = '\0';
            printf("%s\n",InPipe); // 显示读出的数据
            exit(0); // 父进程结束
        } 
    }
}