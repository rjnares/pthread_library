#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

int thread_1(void *vargp)
{
 sleep(5);
 printf("Thread %d: In Thread\n", pthread_self());
 printf("Thread %d: Calling pthread_join() on thread id 2\n", pthread_self());
 pthread_t t2 = 2;
 pthread_join(t2, NULL);
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
 printf("Main: Before Thread\n");
 pthread_create(&t1, NULL, (void * (*)(void*)) thread_1, NULL);
 pthread_create(&t2, NULL, (void * (*)(void*)) thread_2, NULL);
 pthread_join(1, NULL);
 printf("Main: After Threads\n");
 exit(0);
}
