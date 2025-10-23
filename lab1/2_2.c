#include<stdio.h>
#include<pthread.h>
#include<semaphore.h>

int res=0;
sem_t sem;

void *add(void *target){
    for(int i=0;i<100000;i++){
        sem_wait(&sem);
        res+=100;
        sem_post(&sem);
    }
}

void *minus(void *target){
    for(int i=0;i<100000;i++){
        sem_wait(&sem);
        res-=100;
        sem_post(&sem);
    }
}

int main(){
    pthread_t thread1,thread2;
    sem_init(&sem, 0, 1); // 初始化信号量，初始值为1，用于互斥

    if(pthread_create(&thread1,NULL,add,&res)==0){
        printf("Thread 1 created\n");
    }
    if(pthread_create(&thread2,NULL,minus,&res)==0){
        printf("Thread 2 created\n");
    }
    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
    printf("%d\n",res);
    
    sem_destroy(&sem); // 销毁信号量
    return 0;
}