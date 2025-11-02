#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>

int flag=0;
int is_timeout=0;
int child1_isready=0;
int child2_isready=0;

void inter_handler(int sig) {
 // TODO
    if(sig==SIGINT || sig==SIGQUIT){
        flag++;
    }
}

void alarm_handler(int sig) {
 // TODO
    if(sig==SIGALRM){
        is_timeout=1;
        flag=1;
    }
}

void child1_isready_handler(int sig) {
    if(sig==16){
        child1_isready=1;
    }
}

void child2_isready_handler(int sig) {
    if(sig==17){
        child2_isready=1;
    }
}

void waiting() {
 // TODO
    while(flag<1){
        pause();
    }
}

void child_handler(int sig) {
    if(sig==16){
        printf("\nChild process1 is killed by parent!!\n");
        exit(0);
    }
    if(sig==17){
        printf("\nChild process2 is killed by parent!!\n");
        exit(0);
    }
}

int main() {
    signal(SIGQUIT, inter_handler);
    signal(SIGINT, inter_handler);
    //signal(SIGALRM, alarm_handler);
    signal(16, child1_isready_handler);
    signal(17, child2_isready_handler);

    // TODO: 五秒之后或接收到两个信号
    pid_t pid1=-1, pid2=-1;
    while (pid1 == -1)pid1=fork();
    if (pid1 > 0) {
        while (pid2 == -1)pid2=fork();
        if (pid2 > 0) {
            // TODO: 父进程
            alarm(5);
            waiting();
            alarm(0); 
            if(is_timeout){
                //printf("\nThe signal is timeout proc stopped auto!!\n");
            }
            while(!child1_isready){
                pause();
            }
            kill(pid1, 16);
            while(!child2_isready){
                pause();
            }
            kill(pid2, 17);
            wait(NULL);
            wait(NULL);
            
            printf("\nParent process is killed!!\n");
        } else {
            // TODO: 子进程 2
            kill(getppid(), 17);
            signal(17, child_handler);
            while(1){
                pause();
            }
        }
    } else {
        // TODO:子进程 1
        kill(getppid(), 16);
        signal(16, child_handler);
        while(1){
            pause();
        }
        printf("\nChild process1 is killed by parent!!\n");
        return 0;
    }
    return 0;
}