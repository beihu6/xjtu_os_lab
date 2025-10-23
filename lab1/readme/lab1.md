# ����ϵͳʵ�鱨��

# ʵ��һ ���̡��߳���ر�̾���

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

# 1.3 ������ʵ��

## ʵ�鲽��

����һ�� ����ʵ������Ҫ�󣬱�дģ��������������� spinlock.c�� ��������������ʾ���������£�

```c
#include <stdio.h>
#include <pthread.h>
// �����������ṹ��

typedef struct {
    int flag;
}spinlock_t;
// ��ʼ��������

void spinlock_init(spinlock_t *lock) {
    lock->flag = 0;
}

// ��ȡ������
void spinlock_lock(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->flag, 1)) {
    // �����ȴ�
    }
}

// �ͷ�������
void spinlock_unlock(spinlock_t *lock) {
    __sync_lock_release(&lock->flag);
}
// �������

int shared_value = 0;

// �̺߳���

void *thread_function(void *arg) {
    spinlock_t *lock = (spinlock_t *)arg;
 
    for (int i = 0; i < 5000; ++i) {
        spinlock_lock(lock);
        shared_value++;
        spinlock_unlock(lock);
    }
 
    return NULL;
}
int main() {
    pthread_t thread1, thread2;
    spinlock_t lock;
// ������������ֵ
    printf("original shared_value: %d\n", shared_value);
// ��ʼ��������
    spinlock_init(&lock);
// ���������߳�
    if(pthread_create(&thread1, NULL, thread_function, (void *)&lock) != 0) {
        perror("Failed to create thread 1");
        return 1;
    }
    printf("Thread 1 created\n");
    if(pthread_create(&thread2, NULL, thread_function, (void *)&lock) != 0) {
        perror("Failed to create thread 2");
        return 1;
    }
    printf("Thread 2 created\n");
 
// �ȴ��߳̽���
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
// ������������ֵ
    printf("final shared_value: %d\n", shared_value);

    return 0;
}
```
���н��

![Untitled](./picture/step3.png)

���������̹߳���һ����������ÿ��Ҫ�Թ������ִ�в���ʱ����Ҫ��ȡ������˱�֤�˱����İ�ȫ���¡�
