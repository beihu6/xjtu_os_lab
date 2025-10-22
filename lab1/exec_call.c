#include<stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main(){
    printf("exec_call executed,the pid is %d\n",getpid());
}