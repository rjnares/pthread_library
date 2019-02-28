#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

int thread_1(void *vargp)
{
 sleep(5);
 void *return_val;
 printf("Thread %d: In Thread\n", pthread_self());
 printf("Thread %d: Calling pthread_join() on thread id 2\n", pthread_self());
 int succ = pthread_join(2, &return_val);
 printf("Thread %d: return value from pthread_join() call = %d\n", pthread_self(), succ);
 printf("Thread %d: return value from 2 = %d\n", pthread_self(), (int)return_val);
 printf("Thread %d: Exiting\n", pthread_self());
 return 1;
}

int thread_2(void *vargp)
{
 sleep(5);
 printf("Thread %d: In Thread\n", pthread_self());
 printf("Thread %d: Exiting\n", pthread_self());
 return 2;
}

int main() {
 pthread_t t1, t2;
 void *return_val;
 printf("Main: Before Thread\n");
 pthread_create(&t1, NULL, (void * (*)(void*)) thread_1, NULL);
 pthread_create(&t2, NULL, (void * (*)(void*)) thread_2, NULL);
 int succ = pthread_join(1, &return_val);
 printf("Main: After Threads\n");
 printf("Main: return value from pthread_join() call = %d\n", succ);
 printf("Main: return value from 1 = %d\n", (int)return_val);
 exit(0);
}
