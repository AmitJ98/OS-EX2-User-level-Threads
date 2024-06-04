#include <iostream>
#include "uthreads.h"
#define READY 0
#define RUNNING 1
#define BLOCKED 2

static std::list<thread*> ready_queue;
static std::list<thread*> all_threads;
static std::list<bool> available_id(MAX_THREAD_NUM, false);

static thread* running_thread = nullptr;
static int total_quantum = 0;

static struct sigaction sa;
static struct itimerval timer;
static sigset_t set;



//true = unblock , false = block
void block_open(bool op)
{
    if (op)
    {
      sigprocmask (SIG_UNBLOCK, &set, nullptr);
    }
    else
    {
      sigprocmask (SIG_BLOCK, &set, nullptr);
    }
}

void set_id_value(int pos,bool value)
{
  if (pos < available_id.size())
  {
    std::list<bool>::iterator it = available_id.begin();
    std::advance(it, pos);
    *it = value;
  }
}

int check_if_thread_exist(int pos)
{
  if (pos < available_id.size())
  {
    std::list<bool>::iterator it = available_id.begin();
    std::advance(it, pos);
    if (*it == false)
    {
      return -1;
    }
    return 0;
  }
}

int available_index()
{
  int index = 0;
  for (auto it = available_id.begin(); it != available_id.end(); ++it, ++index) {
    if (*it == false) {
      return index;
    }
  }
  return -1;

}

void thread_cleanup()
{
  for (auto it = all_threads.begin(); it != all_threads.end(); ++it) {
      delete (*it);
  }
}

thread* search_thread(int id) {
  for (auto it = all_threads.begin(); it != all_threads.end(); ++it) {
    if ((*it)->get_id() == id) {
      return *it;
    }
  }
  return nullptr;
}

int delete_thread_from_ready_queue(int id)
{
  for (auto it = ready_queue.begin(); it != ready_queue.end(); ++it) {
    if((*it)->get_id() == id )
    {
      ready_queue.erase (it);
      return 0;
    }
  }
  return -1;
}


// 1 = switch , 2 = terminate , 3 = blocked
void switch_threads(int action, int tid)
{
  total_quantum++;
  if (action == 1)
  {
    ready_queue.pop_front ();
    ready_queue.push_back (running_thread);
    running_thread->set_state (READY);

    thread *next = ready_queue.front ();
    next->set_state (RUNNING);

    if ((sigsetjmp(running_thread->_env, 1)) == 0)
    {
      running_thread = next;
      running_thread->increace_quantum_counter ();
      siglongjmp (running_thread->_env, 1);
    }
  }
  if (action == 2)
  {
    set_id_value (tid, false);
    ready_queue.pop_front();
    for (auto it = all_threads.begin(); it != all_threads.end(); ++it)
    {
      if((*it)->get_id() == tid )
      {
        all_threads.erase (it);
        delete *it;
        break;
      }
    }

    running_thread = ready_queue.front();
    running_thread->set_state (RUNNING);
    running_thread->increace_quantum_counter();
    siglongjmp(running_thread->_env,1);

  }
  if (action == 3)
  {
    thread* thread_to_block = ready_queue.back();
    thread_to_block->set_state (BLOCKED);
    ready_queue.pop_back();
  }
}


void time_handler(int sig)
{
  switch_threads(1,0);
}


/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
  if (quantum_usecs <= 0)
  {
    fprintf (stderr,"thread library error: quantum_usecs must be positive integer\n");
    return -1;
  }

  if (sigemptyset(&set) == -1)
  {
    /////////////////////////////// need to print something////////////////////////////
  }
  if (sigaddset(&set, SIGVTALRM) == -1)
  {
    /////////////////////////////// need to print something////////////////////////////
  }

  sa.sa_handler = &time_handler;
  if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
    /////////////////////////////// need to print something////////////////////////////
    return -1;
  }

  timer.it_value.tv_sec = quantum_usecs / 1000000;
  timer.it_value.tv_usec = quantum_usecs % 1000000;

  timer.it_interval.tv_sec = quantum_usecs / 1000000;
  timer.it_interval.tv_usec = quantum_usecs % 1000000;

  set_id_value(0,true);
  thread* main_thread = new thread(0, nullptr);
  main_thread->set_state (RUNNING);
  all_threads.push_back (main_thread);
  ready_queue.push_back (main_thread);
  running_thread = main_thread;
  total_quantum=1;

  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
  {
    return -1;
  }
  return 0;
}


/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point)
{
  block_open (false);
  if (entry_point == nullptr)
  {
    fprintf (stderr,"thread library error: entry point cant be nullptr\n");
    return -1;
  }

  int index  = available_index ();
  if (index == -1)
  {
    fprintf (stderr,"thread library error: reached max threads\n");
    return -1;
  }
  thread* t = new thread(index,entry_point);
  ready_queue.push_back(t);
  all_threads.push_back (t);
  set_id_value (index, true);

  block_open (true);
  return index;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
  block_open (false);

  if(check_if_thread_exist (tid) == -1)
  {
    fprintf (stderr,"something");
    block_open (true);
    return -1;
  }

  if (tid == 0)
  {
    thread_cleanup();
    _exit (0);
  }
  else if (running_thread->get_id() == tid)
  {
    switch_threads (2,tid);
  }
  else
  {
    int thread_to_delete_id = tid;
    set_id_value (thread_to_delete_id, false);
    int res = delete_thread_from_ready_queue (thread_to_delete_id);
    if (res == 0)
    {
      for (auto it = all_threads.begin(); it != all_threads.end(); ++it)
      {
        if((*it)->get_id() == thread_to_delete_id )
        {
          all_threads.erase (it);
          delete *it;
        }
      }
    }
  }
  block_open (true);
  return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
  block_open (false);
  if (check_if_thread_exist (tid) == -1)
  {
    ///////////////////////////////////////
    return -1;
  }

  thread* thread_to_block = search_thread (tid);
  if (thread_to_block == running_thread)
  {
    switch_threads(3,0);
  }

  else if(thread_to_block->get_state() == READY)
  {
    int delete_from_ready = delete_thread_from_ready_queue (tid);
    if (delete_from_ready == -1)
    {
      return -1;
    }
    thread_to_block->set_state (BLOCKED);
  }

  block_open (true);

  return 0;

}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
  block_open (false);

  if (check_if_thread_exist (tid) == -1)
  {
    ///////////////////////////////////////
    return -1;
  }
  thread* thread_to_resume = search_thread (tid);
  if(thread_to_resume->get_state() == BLOCKED)
  {
    thread_to_resume->set_state (READY);
    ready_queue.push_back (thread_to_resume);
  }

  block_open (true);
  return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums);



/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid()
{
  block_open (false);
  int id = running_thread->get_id();
  block_open (true);
  return id;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums()
{
  block_open (false);
  int q = total_quantum -1;////////////////////////
  block_open (true);
  return q;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
  block_open (false);
  int exist = check_if_thread_exist (tid);
  if (exist == -1)
  {
    return -1;
  }
  thread *t  = search_thread (tid);
  int q =  t->get_quantum_counter();
  block_open (true);
  return q;
}