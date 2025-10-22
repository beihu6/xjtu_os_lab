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
		if(execl("/root/code_field/c_code/oslab/lab1/test","test",NULL)==-1){
			printf("execl failed\n");
		}
		printf("if execl succeed,this will not be executed\n");
	}
	else{
		printf("parent:pid=%d\n",getpid());/*C*/
		wait(NULL);
	}
	return 0;
}
