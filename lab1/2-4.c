#include<stdio.h>   
#include<pthread.h>
#include<unistd.h>
#include<sys/types.h>

void* exec_call(void* arg){
    int num=*((int*)arg);
    fprintf(stderr,"Thread%d created\n",num);
    fprintf(stderr,"Thread%d TID:%d,PID:%d\n",num,pthread_self(),getpid());
    execl("/root/code_field/c_code/oslab/lab1/exec_call","exec_call",NULL);
    printf("if execl fails,this line will be printed\n");
}

int main(){
    pthread_t thread1,thread2;   
    int arg1=1,arg2=2;
    if(pthread_create(&thread1,NULL,exec_call,&arg1)!=0){
        printf("Thread 1 creation failed\n");
    }
    if(pthread_create(&thread2,NULL,exec_call,&arg2)!=0){
        printf("Thread 2 creation failed\n");
    }
    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
    printf("if exec succeed,this will no be prinrfed");
}