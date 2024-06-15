#include "uthreads.h"
#include "thread.h"
#include "list"
#include <stdio.h>
#include <signal.h>
#include "algorithm"

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define LIBRARY_ERROR "thread library error:"
#define SYSTEM_ERROR "system error:"

static std::list<thread*> ready_queue;
static std::list<thread*> all_threads;
static std::list<bool> available_id(MAX_THREAD_NUM, false);

static thread* running_thread = nullptr;
static int total_quantum = 0;

static struct sigaction sa;
static struct itimerval timer;
static sigset_t set;



void thread_cleanup()
{
  for (auto it = all_threads.begin(); it != all_threads.end(); ++it) {
    delete (*it);
    (*it) = nullptr;
  }
  all_threads.clear();
  ready_queue.clear();
}

//true = unblock , false = block
void unblock_signals(bool op)
{
  if (op)
  {
    if(sigprocmask (SIG_UNBLOCK, &set, nullptr)==-1){
      fprintf (stderr,SYSTEM_ERROR" unblock signals failed\n");
      thread_cleanup();
      exit(1);
    }
  }
  else
  {
    if(sigprocmask (SIG_BLOCK, &set, nullptr)==-1){
      fprintf (stderr,SYSTEM_ERROR" block signals failed\n");
      thread_cleanup();
      exit(1);
    }
  }
}

void set_id_value(long unsigned pos,bool value)
{
  if (pos < available_id.size())
  {
    std::list<bool>::iterator it = available_id.begin();
    std::advance(it, pos);
    *it = value;
  }
}

int check_if_thread_exist(long unsigned int pos)
{
  if (pos < available_id.size())
  {
    std::list<bool>::iterator it = available_id.begin();
    std::advance(it, pos);
    if (!(*it))
    {
      return -1;
    }
    return 0;
  }
  return -1;
}

//return the first index available for new thread
int available_index()
{
  int index = 0;
  for (auto it = available_id.begin(); it != available_id.end(); ++it, ++index) {
    if (!(*it))
    {
      return index;
    }
  }
  return -1;

}

thread* search_thread(int id) {
  for (auto it = all_threads.begin(); it != all_threads.end(); ++it)
  {
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

void sleep_check()
{
  for (auto it = all_threads.begin(); it != all_threads.end(); ++it)
  {
    if ((*it)->get_sleep())
    {
      (*it)->decreace_time_to_sleep();
      if ((*it)->get_time_to_sleep() == 0)
      {
        (*it)->set_sleep (false);
        if((*it)->get_state() == READY)
        {
          ready_queue.push_back (*it);
        }
      }
    }
  }
}

void block_handler(bool need_to_block)
{
  ready_queue.pop_front();
  if (need_to_block)
  {
    running_thread->set_state (BLOCKED);
  }

  thread *next = ready_queue.front();
  next->set_state (RUNNING);

  if ((sigsetjmp(running_thread->_env, 1)) == 0)
  {
    running_thread = next;
    total_quantum++;
    running_thread->increace_quantum_counter ();
    if(setitimer(ITIMER_VIRTUAL, &timer, NULL)==-1){
      fprintf (stderr,SYSTEM_ERROR" setitimer failed\n");
      thread_cleanup();
      exit(1);
    }
    siglongjmp (running_thread->_env, 1);
  }
}

void terminate_handler(int tid)
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
  total_quantum++;
  running_thread->increace_quantum_counter();
  if(setitimer(ITIMER_VIRTUAL, &timer, NULL)==-1)
  {
    fprintf (stderr,SYSTEM_ERROR" setitimer failed\n");
    thread_cleanup();
    exit(1);
  }
  unblock_signals (true);
  siglongjmp(running_thread->_env,1);
}

void time_handler(int sig)
{
  if(sig == SIGVTALRM)
  {
    sleep_check ();

    ready_queue.pop_front ();
    ready_queue.push_back (running_thread);
    running_thread->set_state (READY);

    thread *next = ready_queue.front ();
    next->set_state (RUNNING);

    if ((sigsetjmp(running_thread->_env, 1)) == 0)
    {
      running_thread = next;
      total_quantum++;
      running_thread->increace_quantum_counter ();
      siglongjmp (running_thread->_env, 1);
    }
  }
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
    fprintf (stderr,LIBRARY_ERROR" quantum_usecs must be positive integer\n");
    return -1;
  }

  if (sigemptyset(&set) == -1)
  {
    fprintf (stderr,SYSTEM_ERROR" sigemptyset failed\n");
    exit(1);

  }
  if (sigaddset(&set, SIGVTALRM) == -1)
  {
    fprintf (stderr,SYSTEM_ERROR" sigaddset failed\n");
    exit(1);
  }

  sa.sa_handler = &time_handler;
  if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
    fprintf (stderr,SYSTEM_ERROR" sigaction failed\n");
    exit(1);
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
  running_thread->increace_quantum_counter();
  total_quantum++;

  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
  {
    fprintf (stderr,SYSTEM_ERROR" setitimer failed\n");
    thread_cleanup();
    exit(1);
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
  unblock_signals (false);
  if (entry_point == nullptr)
  {
    fprintf (stderr,LIBRARY_ERROR" entry point cant be nullptr\n");
    unblock_signals (true);
    return -1;
  }

  int index  = available_index();
  if (index == -1)
  {
    fprintf (stderr,LIBRARY_ERROR" reached max threads\n");
    unblock_signals (true);
    return -1;
  }
  thread* t = new thread(index,entry_point);
  ready_queue.push_back(t);
  all_threads.push_back (t);
  set_id_value (index, true);

  unblock_signals (true);
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
  unblock_signals (false);

  if(check_if_thread_exist (tid) == -1)
  {
    fprintf (stderr,LIBRARY_ERROR" tid don't exist\n");
    unblock_signals (true);
    return -1;
  }

  if (tid == 0)
  {
    thread_cleanup();
    unblock_signals (true);
    exit (0);
  }

  else if (running_thread->get_id() == tid)
  {
    terminate_handler (tid);
    unblock_signals (true); /// Never gets here (doesnt need to)
    return 0;
  }
  else
  {
    set_id_value (tid, false);
    delete_thread_from_ready_queue (tid);
    for (auto it = all_threads.begin(); it != all_threads.end(); ++it)
    {
      if((*it)->get_id() == tid )
      {
        all_threads.erase(it);
        delete *it;
        *it = nullptr;
        break;
      }
    }
    unblock_signals (true);
    return 0;
  }
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
  unblock_signals (false);
  if (tid == 0|| check_if_thread_exist (tid) == -1 )
  {
    if(tid == 0)
    {
      fprintf (stderr,LIBRARY_ERROR" cant block the main thread (tid = 0)\n");
    }
    else
    {
      fprintf (stderr,LIBRARY_ERROR" tid don't exist\n");
    }
    unblock_signals (true);
    return -1;
  }

  thread* thread_to_block = search_thread (tid);
  if (thread_to_block == running_thread)
  {
    block_handler(true);
  }

  else if(thread_to_block->get_state() == READY)
  {
    delete_thread_from_ready_queue (tid);
    thread_to_block->set_state (BLOCKED);
  }

  unblock_signals (true);
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
  unblock_signals (false);

  if (check_if_thread_exist (tid) == -1 )
  {
    fprintf (stderr,LIBRARY_ERROR" tid don't exist\n");
    unblock_signals (true);
    return -1;
  }
  thread* thread_to_resume = search_thread (tid);
  if(thread_to_resume->get_state() == BLOCKED )
  {
    thread_to_resume->set_state (READY);
    if(!thread_to_resume->get_sleep())
    {
      ready_queue.push_back (thread_to_resume);
    }
  }

  unblock_signals (true);
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
int uthread_sleep(int num_quantums)
{
  unblock_signals (false);
  if (running_thread->get_id() == 0 )
  {
    fprintf (stderr,LIBRARY_ERROR" can't put the main thread to sleep \n");
    unblock_signals (true);
    return -1;
  }
  running_thread->set_sleep (true);
  running_thread->set_time_to_sleep(num_quantums);
  running_thread->set_state (READY);
  block_handler(false);
  unblock_signals (true);
  return 0;
}



/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid()
{
  unblock_signals (false);
  int id = running_thread->get_id();
  unblock_signals (true);
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
  unblock_signals (false);
  int q = total_quantum ;
  unblock_signals (true);
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
  unblock_signals (false);
  int exist = check_if_thread_exist (tid);
  if (exist == -1)
  {
    fprintf (stderr,LIBRARY_ERROR" tid don't exist\n");
    unblock_signals (true);
    return -1;
  }
  thread *t  = search_thread (tid);
  int q =  t->get_quantum_counter();
  unblock_signals (true);
  return q;
}