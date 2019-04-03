# pthread_library
User-defined pthread synchronization library.

This library can be integrated into any program that uses pthreads by following these steps:
  1. Make the pthread library object file: g++ -c -o threads.o threads.cpp -m32
  2. Include 'pthread.h' in the source code of the program: #include <pthread.h>
  3. Include the object file in the compilation step of the program: g++ -o my_program my_program.c threads.o -m32

Currently, this library supports:
  1. Thread synchornization functions such as:
    * pthread_create()
    * pthread_exit()
    * pthread_self()
    * pthread_join()

  2. Mutex functions:
    * lock()
    * unlock()

  3. Semaphore functions:
    * sem_init()
    * sem_destroy()
    * sem_wait()
    * sem_post()
