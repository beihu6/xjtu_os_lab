# ����ϵͳʵ�鱨��

# ʵ��һ ���̡��߳���ر�̾���

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
