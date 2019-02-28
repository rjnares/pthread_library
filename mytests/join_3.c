#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

int thread(void *vargp)
{
 sleep(1);
 printf("Thread: In Thread! \n");
 return 15;
}

int main() {
 pthread_t thread_id;
 void *returnValue;
 printf("Main: Before Thread\n");
 pthread_create(&thread_id, NULL, (void * (*)(void*)) thread, NULL);
 pthread_join(thread_id, &returnValue);
 printf("Main: After Thread\n");
 printf("return value: %d\n", (int)returnValue);
 exit(0);
}
