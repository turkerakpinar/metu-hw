#pragma once
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <vector>
extern std::vector<sem_t> sem_responses;
extern int current_obey;
extern timespec current_obey_time;
template<typename T,typename G>
struct timed_arg{
    G* global_data;
    T* request_data;
    long timeout_us;
};
bool time_will_be_done(timespec order_time, timespec waiting_till);
void sleep_till(timespec order_time);
template<typename T>
void* timed_request(void* arg) {
    T* t = (T*)arg;
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    ts.tv_sec += t->timeout_us / 1000000;
    ts.tv_nsec += (t->timeout_us % 1000000) * 1000;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    if(current_obey!= -1 && time_will_be_done(current_obey_time,ts)){
        sleep_till(current_obey_time);
        t->obey = current_obey;
        pthread_exit((void*)1);
    }
    else{
        sleep_till(ts);
        t->obey = -1;
        sem_post(&sem_responses[t->id]);
    }
    pthread_exit(NULL);
}
template<typename T>
void* timed_request_detached(void* arg) {
    pthread_detach(pthread_self());
    T* t = (T*)arg;
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    ts.tv_sec += t->timeout_us / 1000000;
    ts.tv_nsec += (t->timeout_us % 1000000) * 1000;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    if(current_obey!= -1 && time_will_be_done(current_obey_time,ts)){
        sleep_till(current_obey_time);
        t->obey = current_obey;
        pthread_exit((void*)1);
    }
    else{
        sleep_till(ts);
        t->obey = -1;
        sem_post(&sem_responses[t->id]);
    }
    pthread_exit(NULL);
}
