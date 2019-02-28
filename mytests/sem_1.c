#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>
#include <semaphore.h>


sem_t mutex;

int thread(void* arg)
{
    //wait
    sem_wait(&mutex);
    printf("Thread %d: Entered..\n", pthread_self());

    //critical section
    printf("Thread %d: CRITICAL SECTION\n", pthread_self());
    sleep(4);

    //signal
    printf("Thread %d: Just Exiting...\n", pthread_self());
    sem_post(&mutex);

    int ret_val = (int)pthread_self();
    return ret_val;
}


int main()
{
    sem_init(&mutex, 0, 1);
    pthread_t t1,t2;
    void *ret_1, *ret_2;
    pthread_create(&t1,NULL, (void * (*)(void *))thread,NULL);
    sleep(2);
    pthread_create(&t2,NULL, (void * (*)(void *))thread,NULL);
    printf("MAIN: Before join 1\n");
    int join_1 = pthread_join(t1,&ret_1);
    printf("MAIN: After join 1, before join 2\n");
    int join_2 = pthread_join(t2,&ret_2);
    printf("MAIN: After join 2\n");
    int des = sem_destroy(&mutex);
    printf("MAIN: After destroying semaphore\n");
    printf("MAIN: join_1 return value = %d\n", join_1);
    printf("MAIN: join_2 return value = %d\n", join_2);
    printf("MAIN: thread 1 exit status = %d\n", (int)ret_1);
    printf("MAIN: thread 2 exit status = %d\n", (int)ret_2);
    printf("MAIN: semaphore destroy return value = %d\n", des);
    return 0;
}
