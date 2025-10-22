#include<sys/types.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

int main(){
	/*fork a child process*/
	pid_t pid=fork();
	if(pid<0){
		fprintf(stderr,"Fork Failed");
		return 1;
	}
	else if(pid==0){
		printf("child:pid=%d\n",getpid());/*A*/
		int ret=system("./test");
		printf("system call is end,the return value is %d\n",ret);
	}
	else{
		printf("parent:pid=%d\n",getpid());/*C*/
		wait(NULL);
	}
	return 0;
}
