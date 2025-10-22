#include<sys/types.h>
#include<stdio.h>
#include<unistd.h>
int main(){
	printf("hello,I'm the result of calling system or exec");
	printf("\n");
	printf("my pid is %d\n",getpid());
}


