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
    int _time_to_sleep;
    bool _sleep;
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
    bool get_sleep();
    int get_quantum_counter() const;
    int get_time_to_sleep();

    void set_sleep(bool state);
    void set_state(int change_to);
    void increace_quantum_counter();
    void set_time_to_sleep(int q);
    void decreace_time_to_sleep();
};

#endif //_THREAD_H_
