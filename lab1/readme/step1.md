# 1.1 ������ر��ʵ��

##  ʵ�鲽��

��ʵ��ͨ���ڳ�������������ӽ��̵� pid���������ӽ��� pid ֮��Ĺ�ϵ����һ������ wait()�������������á�

����һ�� ��д���������ͼ 1-1 �д���

![Untitled](./picture/step1-1.png)

���Կ�����ʼ�����ȴ�ӡ�ӽ��̣��ٴ�ӡ�����̡�

������� ɾȥͼ 1-1 �����е� wait()������������г��򣬷������н����

![Untitled](./picture/step1-2.png)

���Է��ֵ�ǰ����´���child�п�������parent�������С�

ԭ������wait�����������̵�ִ��֪���ӽ���ִ����ɡ�ȥ��֮�󣬸��ӽ��̲���ִ�У���ӡ˳�򽫲�ȷ����������ֽ�ʬ���̡�

�������� �޸�ͼ 1-1 �д��룬����һ��ȫ�ֱ������ڸ��ӽ����ж�����в�ͬ�Ĳ������۲첢����������������������

![Untitled](lab1/Untitled%203.png)

```c
#include<sys/wait.h>
#include<sys/types.h>
#include<stdio.h>
#include<unistd.h>

int value=0;
int main(){
    pid_t pid,pidi;
    /*fork a child process*/
    pid = fork();
    if(pid<0){
        fprintf(stderr, "Fork Failed");
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
    return 0;
}
```

�Ҷ�����һ��ȫ�ֱ���value����ÿ�������У������ԭʼֵ����������ĺ��ֵ����value�ĸ��Ķ��ǽ�������Ϊ��ǰ���̵�PID��

���н��

![Untitled](./picture/step1-3.png)

���������ӽ����Ⱥ������У��Ǻ���ġ�����ͨ�����������ӽ��̵�value����ӵ��һ�������ĸ�����ÿ�����̶�value���޸��Ƕ����ġ�

�����ģ� �ڲ����������ϣ��� return ǰ���Ӷ�ȫ�ֱ����Ĳ�����������ƣ������������۲첢��������������������

![Untitled](./picture/step1-4.png)

���еĲ����Ƕ�10ȡ�࣬�ٴ�ӡ֤��value�Ƕ����ĸ�����

�����壺 �޸�ͼ 1-1 �������ӽ����е��� system()�� exec �庯���� ��дsystem_call.c �ļ�������̺� PID����������� system_call ��ִ���ļ������ӽ����е��� system_call,�۲��������������ܽᡣ

system.cԴ��

```c
#include<sys/types.h>
#include<stdio.h>
#include<unistd.h>
int main(){
    printf("hello,I'm the result of calling system or exec");
    printf("\n");
    printf("my pid is %d\n",getpid());
}
```

����system��Դ��
```c
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
```

���н��

![Untitled](./picture/step1-5.png)

���Կ���system���½�һ�����̡�

����exec�庯��Դ����
```c
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
```
���н��

![Untitled](./picture/step1-6.png)

�����Ľ�������÷������ӽ��̲�û�����pid�����������޸�

```c
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
        fflush(stdout);
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
```
���н��

![Untitled](./picture/step1-6.png)


### **ʹ�� `system()` ����**

1. **������PID��** �����̵���һ��PID�����Ǹý��̵�Ψһ��ʶ����
2. **�ӽ���PID��** �ӽ���ͨ�� **`system()`** ���� **`system_call`** ִ���ļ�ʱ��������һ���µĽ��̣����½���Ҳ��һ��PID��

### **ʹ�� `exec()` ����**

1. **�ӽ����滻��** ͨ���Ծɴ���ķ���,**`exec()`** �����滻���ӽ��̵�����,��Ҳ�͵�����printfû��������������PID�븸��������ʾ���ӽ���PIDһ�¡�
2. **��������** ���� **`exec()`** �滻���ӽ��̵����ݣ�**`exec()`** ֮����κδ��붼���ᱻִ��,�����н����Ҳ���Կ���exec���һ��printf��û��ִ�С�

### **�ܽ�**

1. **���̶����ԣ�** ʹ�� **`system()`** ������һ��ȫ�µĽ�����ִ�� **`system_call`**���������̺��ӽ��̶�����ִ����ʣ�µĴ��롣
2. **�����滻��** ʹ�� **`exec()`** �滻���ӽ��̵����ݣ������µ� **`system_call`** ������ԭ�ӽ��̵��������У���û�д����µĽ��̡�
3. **��������** ���ַ��������ӽ����гɹ������� **`system_call`**���� **`system()`** �����ӽ��̼���ִ���������룬�� **`exec()`** ����ȫ�滻���ӽ��̣�ʹ�� **`exec()`** ֮��Ĵ��벻�ᱻִ�С�

## 1.1.2ʵ���ܽ�

### 1.1.2.1 ʵ���е�������������

1. **���⣺system() �� exec() ���÷�**
    - **������** �ڳ������ӽ����е��� **`system()`** �� **`exec()`** ����ʱ����������һЩ����Ͳ���Ϥ���÷���
    - **�����** ͨ�������ĵ��Ͳ��ԣ�����������������Ļ����÷������ã����ɹ����ڴ�����Ӧ�������ǡ�

### 1.1.2.2 ʵ���ջ�

1. **���̹�������**��ͨ�����ʵ�飬����������˽��� Linux ϵͳ�н��̵Ĵ���������͵��ȡ��ر���ͨ���۲� **`wait()`** ��������Ϊ������˸��ӽ��̼�ͬ������Ҫ�ԡ�
2. **��̼�������**�����ʵ�����Ҹ���Ϥ�� C ���Եı��ģʽ���������漰��ϵͳ�����úͽ��̹���ĺ������� **`fork()`**, **`wait()`**, **`system()`**, �� **`exec()`** �Ⱥ������˸�������˽⡣
3. **ϵͳ�����������й���**��ʵ�����漰�� **`system()`** �� **`exec()`** ϵ�к�����ʹ���˽�������ڳ�����ִ��ϵͳ����Լ������ **`exec()`** �滻��ǰ���̵�ִ�����ݡ�
4. **����̱��ģ��**��ͨ����һ�������д���������̣��Լ�������Щ���̵���Ϊ��״̬���ҶԶ���̱�����˸�ʵ�ʵ���ʶ����⡣

### 1.1.2.3 ����뽨��

1. **���Ӹ���Ľ��̹���ʵ��**����ǰʵ��������Ȼ�����˻����Ľ��̴����͹�������ʵ��Ӧ���л��и���߼����÷����������̲�����������ͨ�ŵȣ���������ⲿ�����ݡ�
2. **�ṩ����ϸ�ĺ����ĵ���ʾ������**������ʵ���ֲ�����˻�����ܣ���������庯����ʹ�����Ӻ��ĵ��������������⡣
3. **��ǿ�Դ�����Ľ�ѧ**����ʵ�ʱ���У��������Ƿǳ���Ҫ��һ��������ʵ����Ȼ�м򵥵Ĵ�������û����ϸ�����ⷽ������ʵ����
