#include<sys/wait.h>
#include<sys/types.h>
#include<stdio.h>
#include<unistd.h>

int value=0;
int main(){
	pid_t pid,pid1;
	/*fork a child process*/
	pid = fork();
	if(pid<0){
		fprintf(stderr,"Fork Failed");
		return 1;
	}
	else if(pid==0){
		printf("child:pid=%d\n",getpid());
		sleep(2);
		printf("child:original value=%d\n",value);
		value=getpid();
		printf("child:updated value=%d\n",value);
	}

	else{
		printf("father:pid=%d\n",getpid());
		printf("father:original value=%d\n",value);
		value=getpid();
		printf("father:updated value=%d\n",value);
		wait(NULL);
	}

	printf("before return\n");
	printf("----------------\n");

	value=value%10;
	printf("%d\n",value);
	return 0;
}
