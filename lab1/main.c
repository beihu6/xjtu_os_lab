#include<sys/types.h>
#include<stdio.h>
#include<unistd.h>

int main(){
	pid_t pid,pid1;
	/*fork a child process*/
	pid = fork();
	if(pid<0){
		fprintf(stderr,"Fork Failed");
		return 1;
	}
	else if(pid==0){
		pid1 = getpid();
		printf("child:pid=%d\n",pid);/*A*/
		printf("child:pid1=%d\n",pid1);/*B*/
	}
	else{
		pid1=getpid();
		printf("parent:pid=%d\n",pid);/*C*/
		printf("parent:pid1=%d\n",pid1);/*D*/
	}
	return 0;
}
