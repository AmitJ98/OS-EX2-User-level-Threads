#include "thread.h"
#define STACK_SIZE 4096
#define JB_SP 6
#define JB_PC 7

#define READY 0
#define RUNNING 1
#define BLOCKED 2

#ifdef __x86_64__
/* code for 64 bit Intel arch */

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#endif





thread::thread (int id , thread_entry_point f)
{
  _id = id;
  _state = READY;
  _sleep = false;
  _time_to_sleep = 0;
  _quantum_counter = 0;
  _f = f;
  _stack = new char[STACK_SIZE];
  _sp = (address_t) _stack + STACK_SIZE - sizeof(address_t);
  _pc = (address_t)f;

  sigsetjmp(_env, 1);
  (_env->__jmpbuf)[JB_SP] = translate_address(_sp);
  (_env->__jmpbuf)[JB_PC] = translate_address(_pc);
  sigemptyset(&_env->__saved_mask);

}

thread::~thread ()
{
  delete [] _stack;
  _stack = nullptr;
}

int thread::get_id ()const
{
  return _id;
}

int thread::get_state ()const
{
  return _state;
}

int thread::get_quantum_counter () const
{
  return _quantum_counter;
}

bool thread::get_sleep ()
{
  return _sleep;
}

int thread::get_time_to_sleep ()
{
  return _time_to_sleep;
}


void thread::set_state (int change_to)
{
  _state = change_to;
}

void thread::set_sleep (bool state)
{
  _sleep = state;
}

void thread::set_time_to_sleep (int q)
{
  _time_to_sleep = q;

}

void thread::increace_quantum_counter ()
{
  _quantum_counter++;
}

void thread::decreace_time_to_sleep ()
{
  _time_to_sleep--;
}
