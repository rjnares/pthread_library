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
  void *ret_val;
  printf("Main: Before new thread\n");
  pthread_create(&t1, NULL, start_func, NULL);
  pthread_join(t1, &ret_val);
  printf("Main: After thread terminates\n");
  if (ret_val == NULL) {
      printf("Main: Got exit status\n");
  } else {
      printf("Main: Didn't get anything\n");
  }
  exit(0);
}
