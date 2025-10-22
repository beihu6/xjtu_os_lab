#include<stdio.h>
#include<pthread.h>

int res=0;

void *add(void *target){
	for(int i=0;i<100000;i++){
		res+=100;
	}
}

void *minus(void *target){
	for(int i=0;i<100000;i++){
		res-=100;
	}
}

int main(){
	pthread_t thread1,thread2;
	if(pthread_create(&thread1,NULL,add,&res)==0){
		printf("Thread 1 created\n");
	}
	if(pthread_create(&thread2,NULL,minus,&res)==0){
		printf("Thread 2 created\n");
	}
	pthread_join(thread1,NULL);
	pthread_join(thread2,NULL);
	printf("%d\n",res);
	return 0;
}
