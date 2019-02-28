#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void *start_func(void *arg) {
  sleep(5);
  printf("Thread: running before pthread join call can end\n");
  sleep(5);
  printf("Thread: exiting\n");
  return NULL;
}


int main(int argc, char **argv) {
  pthread_t t1;
  printf("Main: Before new thread\n");
  pthread_create(&t1, NULL, start_func, NULL);
  pthread_join(t1, NULL);
  printf("Main: After thread terminates\n");
  exit(0);
}
