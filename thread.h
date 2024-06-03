#ifndef _THREAD_H_
#define _THREAD_H_

#include <setjmp.h>
#include <sys/time.h>
#include <signal.h>

typedef void (*thread_entry_point)(void);
typedef unsigned long long address_t; //?



class thread
{
  private:
    int _id;
    int _state; // 0 - ready, 1 - running , 2 - blocked
    int _quantum_counter;
    address_t _sp;
    address_t _pc;
    char *_stack;
    void (*_f)();



 public:
    sigjmp_buf _env;
    thread(int id , void (*f)(void) );
    ~thread();


    int get_id() const;
    int get_state()const;
    int get_quantum_counter() const;


    void set_state(int change_to);
    void increace_quantum_counter();
};

#endif //_THREAD_H_
