#include "request_response.h"
bool time_will_be_done(timespec order_time, timespec waiting_till){
    if(waiting_till.tv_sec>order_time.tv_sec){
        return true;
    }
    else if(waiting_till.tv_sec==order_time.tv_sec){
        if(waiting_till.tv_nsec>order_time.tv_nsec){
            return true;
        }
    }
    return false;
}
void sleep_till(timespec order_time){
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    long seconds_to_sleep = order_time.tv_sec - ts.tv_sec;
    long nanoseconds_to_sleep = order_time.tv_nsec - ts.tv_nsec;
    if(nanoseconds_to_sleep<0){
        nanoseconds_to_sleep += 1000000000L;
        seconds_to_sleep--;
    }
    if(seconds_to_sleep<0 || nanoseconds_to_sleep<0){
        return;
    }
    usleep(seconds_to_sleep*1000000+nanoseconds_to_sleep/1000);
}