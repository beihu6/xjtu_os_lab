# ����ϵͳʵ�鱨��

# ʵ��һ ���̡��߳���ر�̾���

# 1.2 �߳���ر��ʵ��

## ʵ�鲽��

����һ�� ��Ƴ��򣬴����������̣߳� ���̷ֱ߳��ͬһ�����������β������۲���������

Դ�������£�
```c
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
```

���н��

![Untitled](./picture/step2-1.png)

���Է��֣���ʱ����Ľ�����������̶���������Ϊ�����̲߳���ִ�У��Թ������res��ȡ���Ĵ�����ʱ����ȡ������ͬһ��ֵ��һ���߳�Ҫ+100��һ���߳�Ҫ-100������Ч��Ӧ�����໥�����ģ���д���ڴ�ʱ��ֻ���Լ�������Ľ��д���ڴ棬��ʱ�ͱ��ֳ�+100��-100������ֵ�ĸı䡣

����һ���򵥵��龳��

1. **res** ��ǰֵΪ 0��
2. �߳�1 ��ȡ��ֵ��0����׼����1��
3. �߳�2 �����ȣ���ȡ��ֵ������0����׼����1��
4. �߳�1 ����ִ�У���ֵ��1�����Ϊ1��
5. �߳�2 ����ִ�У���1����Ϊ֮ǰ��ֵ��0�������Ϊ -1��

������� �޸ĳ��� �����ź��� signal��ʹ�� PV ����ʵ�ֹ�������ķ����뻥�⡣���г��򣬹۲����չ��������ֵ��

ͬ����Synchronization���ڶ��̱߳����ָ����Э������̵߳�ִ�У���ȷ�������ܹ���������Ԥ��ط��ʹ�����Դ�����ĳЩ���񡣻��⣨Mutual Exclusion����ͬ����һ��������ʽ��ȷ��һ��ֻ��һ���߳��ܷ���ĳ���ض�����Դ�����Ρ�����ͨ��ͨ����������Mutex����ʵ�֣�����Ҳ����ͨ������������ʵ�֣������ź�����Semaphore�����ź����ͻ��������ƣ�����Ϊͨ�á��ź�����������������˻���֮�������ͬ�����⡣

PV�������ź�����Semaphores�������Ĵ�ͳ���Դ�Ժ������Proberen�����ԣ���Verhogen�����ӣ������ź������������У�P����ͨ�����������ȴ���Դ����V���������ͷŻ򷢳��źš�

**P������Ҳ��`wait`��`down`��`sem_wait`��**

- ��һ���߳�ִ��P����ʱ���������ź�����ֵ��
    - ����ź�����ֵ����0����ô���������ź�����ֵ��ͨ���Ǽ�1��������ִ�С�
    - ����ź�����ֵΪ0���߳̽���������ֱ���ź�����ֵ��Ϊ����0��

**V������Ҳ��`signal`��`up`��`sem_post`��**

- V���������ź�����ֵ��ͨ���Ǽ�1����
- ������߳���ִ��P����������������ź����ϣ�һ�������߳̽������������������������ź�����ֵ��

��C�����У�ʹ��POSIX�ź���

- **`sem_wait(&semaphore);`**��ִ��P������
- **`sem_post(&semaphore);`**��ִ��V������

���У�**`semaphore`**��һ��**`sem_t`**���͵ı����������ź�����

**`sem_init`**��**`sem_destroy`**��POSIX�ź�����Semaphores���ĳ�ʼ�������ٺ����������������ú������ź�����

**`sem_init`**

����������ڳ�ʼ��һ��δ�������ź�����

```c
int sem_init(sem_t *sem, int pshared, unsigned int value);
```

- **sem**: һ��ָ���ź��������ָ�롣
- **pshared**: ������������0���ź������ǵ�ǰ���̵ľֲ��ź�����������������0������ź����ڶ�����̼乲��
- **value**: �ź����ĳ�ʼֵ��

��������ɹ�ʱ����0��ʧ��ʱ����-1��

**`sem_destroy`**

���������������һ��δ�������ź������ͷ���ռ�õ���Դ��

```c
int sem_destroy(sem_t *sem);
```

- **sem**: һ��ָ���ź��������ָ�롣

��������ɹ�ʱ����0��ʧ��ʱ����-1����ʹ��**`sem_destroy`**֮ǰ��ȷ��û���̱߳������ڸ��ź����ϡ�

����Ĵ���Ϊ��

```c
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
    sem_init(&sem, 0, 1); // ��ʼ���ź�������ʼֵΪ1�����ڻ���

    if(pthread_create(&thread1,NULL,add,&res)==0){
        printf("Thread 1 created\n");
    }
    if(pthread_create(&thread2,NULL,minus,&res)==0){
        printf("Thread 2 created\n");
    }
    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
    printf("%d\n",res);
    
    sem_destroy(&sem); // �����ź���
    return 0;
}
```
���н��

![Untitled](./picture/step2-2.png)


���Կ������ź���ͬ��֮�󣬽��������ȷ�ģ���Ԥ��ġ�

�������� �ڵ�һ����ʵ���˽��� system()�� exec �庯���Ļ����ϣ��������������ĵ��ø�Ϊ���߳���ʵ�֣�������� PID ���̵߳� TID ���з�����

system_call����Դ��
```c
#include<stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main(){
    printf("thread_system_call pid:%d\n",getpid());
}
```

����system�ĺ���Դ��
```c
#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include <sys/types.h>

void* system_call(void* arg){
    int num=*((int*)arg);
    printf("Thread%d created\n",num);
    printf("Thread%d TID:%d, PID:%d\n",num,pthread_self(),getpid());
    system("/root/code_field/c_code/oslab/lab1/thread_system_call");
}

int main(){
    pthread_t thread1,thread2;
    int arg1=1,arg2=2;
    if(pthread_create(&thread1,NULL,system_call,&arg1)!=0){
        printf("Thread 1 creation failed\n");
    }
    if(pthread_create(&thread2,NULL,system_call,&arg2)!=0){
        printf("Thread 2 creation failed\n");
    }
    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
}
```
���н��

![Untitled](./picture/step2-3.png)

�����ʾ�̵߳�TID��ͬ��������һ����ͬ��PID��Ҳ���Ǹ����̵�PID��
ÿһ���̵߳�system���ö��ᴴ��һ���½���

Ӧ��exec�庯��Դ��


```c
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
```
���н��

![Untitled](./picture/step2-4.png)

������������̵߳����һ�е�printf��û��ִ�У�����execֻ�еĴ���ʵ����ִֻ����һ�飬������Ϊexec�ĵ���ʵ�����ǽ��̼���ģ�����exec�ᵼ���������̵Ĵ���α��滻�����������ǵ�ǰ�߳�ִ�еĺ�����