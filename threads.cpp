/*
 * CS170 - Operating Systems
 * Project 3 Solution
 * Authors: CS170 TAs, Roberto J. Nares, and Colin Mai
 */


#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <queue>
#include <map> // NEW
#include <semaphore.h> // NEW
#include <vector> // NEW
#include <utility> // NEW


/* 
 * these could go in a .h file but i'm lazy 
 * see comments before functions for detail
 */
void signal_handler(int signo);
void the_nowhere_zone(void);
static int ptr_mangle(int p);

/* NEW */
void pthread_exit_wrapper();
void lock();
void unlock();
int pthread_join(pthread_t thread, void **value_ptr);
int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);


/* 
 *Timer globals 
 */
static struct timeval tv1,tv2;
static struct itimerval interval_timer = {0}, current_timer = {0}, zero_timer = {0};
static struct sigaction act;



/*
 * Timer macros for more precise time control 
 */

#define PAUSE_TIMER setitimer(ITIMER_REAL,&zero_timer,&current_timer)
#define RESUME_TIMER setitimer(ITIMER_REAL,&current_timer,NULL)
#define START_TIMER current_timer = interval_timer; setitimer(ITIMER_REAL,&current_timer,NULL)
#define STOP_TIMER setitimer(ITIMER_REAL,&zero_timer,NULL)
/* number of ms for timer */
#define INTERVAL 50


/*
 * Thread Control Block definition 
 */
typedef struct {
  /* pthread_t usually typedef as unsigned long int */
  pthread_t id;
  /* jmp_buf usually defined as struct with __jmpbuf internal buffer
     which holds the 6 registers for saving and restoring state */
  jmp_buf jb;
  /* stack pointer for thread; for main thread, this will be NULL */	
  char *stack;

  /* NEW */
  bool is_locked;

  /* NEW */
  void *exit_status;

  /* NEW */
  bool is_blocked;
  
} tcb_t;

/* NEW */
typedef struct {
  int val;
  std::queue<tcb_t> *wait_q;
  bool init = false;
  
} semaphore;



/* 
 * Globals for thread scheduling and control 
 */

/* queue for pool thread, easy for round robin */
static std::queue<tcb_t> thread_pool;
/* keep separate handle for main thread */
static tcb_t main_tcb;
static tcb_t garbage_collector;

/* for assigning id to threads; main implicitly has 0 */
static unsigned long id_counter = 1; 
/* we initialize in pthread_create only once */
static int has_initialized = 0;

/* NEW */
static std::map<pthread_t, tcb_t> zombie_map;

/* NEW */
static std::map<pthread_t, pthread_t> blocked_threads;

static std::vector<pthread_t> threads_to_be_unblocked;

#define SEM_VALUE_MAX 65536 // NEW



/*
 * init()
 *
 * Initialize thread subsystem and scheduler
 * only called once, when first initializing timer/thread subsystem, etc... 
 */
void init() {
	/* on signal, call signal_handler function */
	act.sa_handler = signal_handler;
	/* set necessary signal flags; in our case, we want to make sure that we intercept
	   signals even when we're inside the signal_handler function (again, see man page(s)) */
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NODEFER;

	/* register sigaction when SIGALRM signal comes in; shouldn't fail, but just in case
	   we'll catch the error  */
	if(sigaction(SIGALRM, &act, NULL) == -1) {
		perror("Unable to catch SIGALRM");
		exit(1);
	}

	/* set timer in seconds */
	interval_timer.it_value.tv_sec = INTERVAL/1000;
	/* set timer in microseconds */
	interval_timer.it_value.tv_usec = (INTERVAL*1000) % 1000000;
	/* next timer should use the same time interval */
	interval_timer.it_interval = interval_timer.it_value;

	/* create thread control buffer for main thread, set as current active tcb */
	main_tcb.id = 0;
	main_tcb.stack = NULL;

	/* NEW */
	main_tcb.is_locked = false;

	/* NEW */
	main_tcb.exit_status = NULL;

	/* NEW */
	main_tcb.is_blocked = false;
	
	/* front of thread_pool is the active thread */
	thread_pool.push(main_tcb);

	/* set up garbage collector */
	garbage_collector.id = 128;
	garbage_collector.stack = (char *) malloc (32767);

	/* initialize jump buf structure to be 0, just in case there's garbage */
	memset(&garbage_collector.jb,0,sizeof(garbage_collector.jb));
	/* the jmp buffer has a stored signal mask; zero it out just in case */
	sigemptyset(&garbage_collector.jb->__saved_mask);

	/* garbage collector 'lives' in the_nowhere_zone */
	garbage_collector.jb->__jmpbuf[4] = ptr_mangle((uintptr_t)(garbage_collector.stack+32759));
	garbage_collector.jb->__jmpbuf[5] = ptr_mangle((uintptr_t)the_nowhere_zone);

	/* Initialize timer and wait for first sigalarm to go off */
	START_TIMER;
	pause();	
}



/* 
 * pthread_create()
 * 
 * create a new thread and return 0 if successful.
 * also initializes thread subsystem & scheduler on
 * first invocation 
 */
int pthread_create(pthread_t *restrict_thread, const pthread_attr_t *restrict_attr,
                   void *(*start_routine)(void*), void *restrict_arg) {
  
  /* set up thread subsystem and timer */
  if(!has_initialized) {
    has_initialized = 1;
    init();
  }
  
  /* pause timer while creating thread */
  PAUSE_TIMER;
  
  /* create thread control block for new thread
     restrict_thread is basically the thread id 
     which main will have access to */
  tcb_t tmp_tcb;
  tmp_tcb.id = id_counter++;
  *restrict_thread = tmp_tcb.id;
  
  /* NEW */
  tmp_tcb.is_locked = false;
  
  /* NEW */
  tmp_tcb.exit_status = NULL;
  
  /* NEW */
  tmp_tcb.is_blocked = false;
  
  /* simulate function call by pushing arguments and return address to the stack
     remember the stack grows down, and that threads should implicitly return to
     pthread_exit after done with start_routine */
  
  tmp_tcb.stack = (char *) malloc (32767);
  
  *(int*)(tmp_tcb.stack+32763) = (int)restrict_arg;
  *(int*)(tmp_tcb.stack+32759) = (int)pthread_exit_wrapper; // NEW
  
  /* initialize jump buf structure to be 0, just in case there's garbage */
  memset(&tmp_tcb.jb,0,sizeof(tmp_tcb.jb));
  /* the jmp buffer has a stored signal mask; zero it out just in case */
  sigemptyset(&tmp_tcb.jb->__saved_mask);
  
  /* modify the stack pointer and instruction pointer for this thread's
     jmp buffer. don't forget to mangle! */
  tmp_tcb.jb->__jmpbuf[4] = ptr_mangle((uintptr_t)(tmp_tcb.stack+32759));
  tmp_tcb.jb->__jmpbuf[5] = ptr_mangle((uintptr_t)start_routine);
  
  /* new thread is ready to be scheduled! */
  thread_pool.push(tmp_tcb);
  
  /* resume timer */
  RESUME_TIMER;
  
  return 0;	
}



/* 
 * pthread_self()
 *
 * just return the current thread's id
 * undefined if thread has not yet been created
 * (e.g., main thread before setting up thread subsystem) 
 */
pthread_t pthread_self(void) {
  if(thread_pool.size() == 0) {
    return 0;
  } else {
    return (pthread_t)thread_pool.front().id;
  }
}



/* 
 * pthread_exit()
 *
 * pthread_exit gets returned to from start_routine
 * here, we should clean up thread (and exit if no more threads) 
 */
void pthread_exit(void *value_ptr) {  
  /* just exit if not yet initialized */
  if(has_initialized == 0) {
    exit(0);
  }
  
  /* stop the timer so we don't get interrupted */
  STOP_TIMER;
  
  if(thread_pool.front().id == 0) {
    /* if its the main thread, still keep a reference to it
       we'll longjmp here when all other threads are done */
    main_tcb = thread_pool.front();
    if(setjmp(main_tcb.jb)) {
      /* garbage collector's stack should be freed by OS upon exit;
	 We'll free anyways, for completeness */
      free((void*) garbage_collector.stack);
      exit(0);
    } 
  }
  
  /* NEW */
  thread_pool.front().exit_status = value_ptr;

  /* Jump to garbage collector stack frame to free memory and scheduler another thread.
     Since we're currently "living" on this thread's stack frame, deleting it while we're
     on it would be undefined behavior */
  longjmp(garbage_collector.jb,1); 
}


/* 
 * signal_handler()
 * 
 * called when SIGALRM goes off from timer 
 */
void signal_handler(int signo) {
  /* if no other thread, just return */
  if(thread_pool.size() <= 1) {
    return;
  }
  
  /* Time to schedule another thread! Use setjmp to save this thread's context
     on direct invocation, setjmp returns 0. if jumped to from longjmp, returns
     non-zero value. */
  if(setjmp(thread_pool.front().jb) == 0) {

    STOP_TIMER; // NEW
    
    /* switch threads */   
    thread_pool.push(thread_pool.front());
    thread_pool.pop();

    /* NEW */
    bool found = false;
      std::map<pthread_t, pthread_t>::iterator block_it = blocked_threads.begin();
      while (block_it != blocked_threads.end() && !found) {
	if (block_it->second == pthread_self()) {
	  for (std::map<pthread_t, tcb_t>::iterator zombie_it = zombie_map.begin(); zombie_it != zombie_map.end(); zombie_it++) {
	    if (zombie_it->first == block_it->first) {
	      thread_pool.front().is_blocked = false;
	      blocked_threads.erase(block_it);
	      found = true;
	      break;
	    }
	  }
	}
	++block_it;
      }

    
    /* NEW */
    for (std::vector<pthread_t>::iterator vec_it = threads_to_be_unblocked.begin(); vec_it != threads_to_be_unblocked.end(); ++vec_it) {
      if ((*vec_it) == pthread_self()) {
	thread_pool.front().is_blocked = false;
	threads_to_be_unblocked.erase(vec_it);
	break;
      }
    }
      
    
    /* NEW */
    while (thread_pool.front().is_blocked) {

      thread_pool.push(thread_pool.front());
      thread_pool.pop();

      /* NEW */
      for (std::vector<pthread_t>::iterator vec_it = threads_to_be_unblocked.begin(); vec_it != threads_to_be_unblocked.end(); ++vec_it) {
	if ((*vec_it) == pthread_self()) {
	  thread_pool.front().is_blocked = false;
	  threads_to_be_unblocked.erase(vec_it);
	  break;
	}
      }

      /* NEW */
      found = false;
      std::map<pthread_t, pthread_t>::iterator block_it = blocked_threads.begin();
      while (block_it != blocked_threads.end() && !found) {
	if (block_it->second == pthread_self()) {
	  for (std::map<pthread_t, tcb_t>::iterator zombie_it = zombie_map.begin(); zombie_it != zombie_map.end(); zombie_it++) {
	    if (zombie_it->first == block_it->first) {
	      thread_pool.front().is_blocked = false;
	      blocked_threads.erase(block_it);
	      found = true;
	      break;
	    }
	  }
	}
	++block_it;
      }
      
    }

    START_TIMER; // NEW
    
    /* resume scheduler and GOOOOOOOOOO */
    longjmp(thread_pool.front().jb,1);
  }
  
  /* resume execution after being longjmped to */
  return;
}


/* 
 * the_nowhere_zone()
 * 
 * used as a temporary holding space to safely clean up threads.
 * also acts as a pseudo-scheduler by scheduling the next thread manually
 */
void the_nowhere_zone(void) {
  /* free stack memory of exiting thread 
     Note: if this is main thread, we're OK since
     free(NULL) works */ 
  free((void*) thread_pool.front().stack);
  thread_pool.front().stack = NULL;

  /* NEW */
  zombie_map[thread_pool.front().id] = thread_pool.front();
  // pthread_t zombie_id = thread_pool.front().id; remove
  
  /* Don't schedule the thread anymore */
  thread_pool.pop();
  
  /* NEW */
  bool found = false;
  std::map<pthread_t, pthread_t>::iterator block_it = blocked_threads.begin();
  while (block_it != blocked_threads.end() && !found) {
    if (block_it->second == pthread_self()) {
      for (std::map<pthread_t, tcb_t>::iterator zombie_it = zombie_map.begin(); zombie_it != zombie_map.end(); zombie_it++) {
	if (zombie_it->first == block_it->first) {
	  thread_pool.front().is_blocked = false;
	  blocked_threads.erase(block_it);
	  found = true;
	  break;
	}
      }
    }
    ++block_it;
  }

  /* NEW */
  for (std::vector<pthread_t>::iterator vec_it = threads_to_be_unblocked.begin(); vec_it != threads_to_be_unblocked.end(); ++vec_it) {
    if ((*vec_it) == pthread_self()) {
      thread_pool.front().is_blocked = false;
      threads_to_be_unblocked.erase(vec_it);
      break;
    }
  }
  
  
  /* remove
  std::map<pthread_t, pthread_t>::iterator block_it = blocked_threads.find(zombie_id);
  if (block_it != blocked_threads.end() && thread_pool.front().id == block_it->second) {
    thread_pool.front().is_blocked = false;
    blocked_threads.erase(block_it);
  }
  */
  
  /* If the last thread just exited, jump to main_tcb and exit.
     Otherwise, start timer again and jump to next thread*/
  if(thread_pool.size() == 0) {
    longjmp(main_tcb.jb,1);
  } else {
    
    /* NEW */
    while (thread_pool.front().is_blocked) {
      thread_pool.push(thread_pool.front());
      thread_pool.pop();

      /* NEW */
      found = false;
      std::map<pthread_t, pthread_t>::iterator block_it = blocked_threads.begin();
      while (block_it != blocked_threads.end() && !found) {
	if (block_it->second == pthread_self()) {
	  for (std::map<pthread_t, tcb_t>::iterator zombie_it = zombie_map.begin(); zombie_it != zombie_map.end(); zombie_it++) {
	    if (zombie_it->first == block_it->first) {
	      thread_pool.front().is_blocked = false;
	      blocked_threads.erase(block_it);
	      found = true;
	      break;
	    }
	  }
	}
	++block_it;
      }

      /* NEW */
      for (std::vector<pthread_t>::iterator vec_it = threads_to_be_unblocked.begin(); vec_it != threads_to_be_unblocked.end(); ++vec_it) {
	if ((*vec_it) == pthread_self()) {
	  thread_pool.front().is_blocked = false;
	  threads_to_be_unblocked.erase(vec_it);
	  break;
	}
      }

      
      /* remove
      for (std::map<pthread_t, pthread_t>::iterator block_it = blocked_threads.begin(); block_it != blocked_threads.end(); ++block_it) {
	if (block_it->second == pthread_self()) {
	  for (std::map<pthread_t, tcb_t>::iterator zombie_it = zombie_map.begin(); zombie_it != zombie_map.end(); ++zombie_it) {
	    if (zombie_it->first == block_it->first) {
	      thread_pool.front().is_blocked= false;
	      blocked_threads.erase(block_it);
	    }
	  }
	}
      }
      */
      /* remove
      std::map<pthread_t, pthread_t>::iterator block_it = blocked_threads.find(zombie_id);
      if (block_it != blocked_threads.end() && thread_pool.front().id == block_it->second) {
	thread_pool.front().is_blocked = false;
	blocked_threads.erase(block_it);
      }
      */
    }
    
    START_TIMER;
    longjmp(thread_pool.front().jb,1);
  }
}

/* 
 * ptr_mangle()
 *
 * ptr mangle magic; for security reasons 
 */
int ptr_mangle(int p)
{
    unsigned int ret;
    __asm__(" movl %1, %%eax;\n"
        " xorl %%gs:0x18, %%eax;"
        " roll $0x9, %%eax;"
        " movl %%eax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%eax"
    );
    return ret;
}

/* NEW */
void pthread_exit_wrapper() {
  unsigned int res;
  asm("movl %%eax, %0\n":"=r"(res));
  pthread_exit((void *) res);
}


/* NEW */
void lock() {
  // create new set of signals
  sigset_t set;

  // empty the newly created set of signals
  if (sigemptyset(&set) == -1) {
    perror("Failed to empty the signal set");
    exit(1);
  }

  // add SIGALRM to the empty set of signals; 'set' now only contains SIGALRM
  if (sigaddset(&set, SIGALRM) == -1) {
    perror("Failed to add SIGALRM to the signal set");
    exit(1);
  }

  // set the current signal mask to 'set' but save old signal mask in 'act' sigaction structure
  if (sigprocmask(SIG_SETMASK, &set, &act.sa_mask) == -1) {
    perror("Failed to change the signal mask");
    exit(1);
  }

  // set current thread's lock status to 'true' for use in unlock()
  thread_pool.front().is_locked = true;
}

/* NEW */
void unlock() {
  // check if current thread has been previously locked
  if (thread_pool.front().is_locked) {
    // set the current signal mask to old signal mask in 'act' sigaction structure
    if(sigprocmask(SIG_SETMASK, &act.sa_mask, NULL) == -1) {
      perror("Failed to change the signal mask");
      exit(1);
    }

    // set current thread's lock status to 'false'
    thread_pool.front().is_locked = false;
  }
  
  // simply exit the function and do nothing if current thread has not been previously locked
}

/* NEW */
int pthread_join(pthread_t thread, void **value_ptr) {
  lock();
  std::map<pthread_t, tcb_t>::iterator zombie_it = zombie_map.find(thread);
  if (zombie_it != zombie_map.end()) {
    if (value_ptr != NULL) {
      *value_ptr = zombie_it->second.exit_status;
    }
    zombie_map.erase(zombie_it);
    unlock();
    return 0;
  } else {
    thread_pool.front().is_blocked = true;
    blocked_threads[thread] = thread_pool.front().id;
  }
  unlock();
  pause();
  lock();
  zombie_it = zombie_map.find(thread);
  if (zombie_it != zombie_map.end()) {
    if (value_ptr != NULL) {
      *value_ptr = zombie_it->second.exit_status;
    }
    zombie_map.erase(zombie_it);
    unlock();
    return 0;
  }
  unlock();
  return 1;
}

/* NEW */
int sem_init(sem_t *sem, int pshared, unsigned value) {
  lock();
  if (pshared != 0 || value >= SEM_VALUE_MAX || value == 0 || sem == NULL) {
    unlock();
    return -1;
  }
  semaphore *new_sem = (semaphore *)malloc(sizeof(semaphore));
  if (new_sem == NULL) {
    perror("Failed to allocate memory for new semaphore structure");
    return -1;
  }
  new_sem->val = value;
  new_sem->wait_q = new std::queue<tcb_t>; // NEW: for some reason fixes segmentation fault when adding tcb_t pointer
  /*
  new_sem->wait_q = (std::queue<tcb_t> *)malloc(sizeof(std::queue<tcb_t>));
  if (new_sem->wait_q == NULL) {
    perror("Failed to allocate memory for new queue structure");
    return -1;
  }
  */
  new_sem->init = true;
  sem->__align = (long int)new_sem;
  unlock();
  return 0;
}

/* NEW */
int sem_destroy(sem_t *sem) {
  lock();
  if (sem == NULL) {
    unlock();
    return -1;
  }
  semaphore *temp = (semaphore *)sem->__align;
  if (!temp->init) {
    unlock();
    return -1;
  }
  free(temp->wait_q);
  free(temp);
  unlock();
  return 0;
}

/* NEW */
int sem_wait(sem_t *sem) {
  lock();
  if (sem == NULL) {
    unlock();
    return -1;
  }
  semaphore *temp = (semaphore *)sem->__align;
  if (temp->val > 0) {
    temp->val--;
    unlock();
    return 0;
  } else if (temp->val == 0) {
    thread_pool.front().is_blocked = true;
    temp->wait_q->push(thread_pool.front());
    unlock();
    pause();
    return 0;
  } else {
    unlock();
    return -1;
  }
}

/* NEW */
int sem_post(sem_t *sem) {
  lock();
  if (sem == NULL) {
    unlock();
    return -1;
  }
  semaphore *temp = (semaphore *)sem->__align;
  temp->val++;
  if (temp->val == 1 && !(temp->wait_q->empty())) {
    threads_to_be_unblocked.push_back(temp->wait_q->front().id);
    temp->wait_q->pop();
    temp->val--;
    unlock();
    pause();
    return 0;
  }
  unlock();
  return 0;
}
