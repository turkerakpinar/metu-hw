#include <iostream>
#include <vector>
#include <array>
#include <pthread.h>
#include "hw2_output.h"
#include <string.h>
#include <unistd.h>
#include "request_response.h"
using namespace std;

class proper
{
private:
    /* data */
public:
    int gid, s_i, s_j, t_g, n_g, id;
    vector<vector<int>> areas;
    proper(/* args */);
    ~proper();
};

proper::proper(/* args */)
{
}

proper::~proper()
{
}
class sneaky
{
private:
    /* data */
public:
    int sid, time_to_smoke, cell_count, id;
    vector<array<int, 3>> sneaky_area;
};
class obey
{
private:
    /* data */
public:
    int time, obey;
    // 0 break
    // 1 contunie
    // 2 stop
};
class request_order
{
public:
    int id;
    long timeout_us;
    int obey;
};

int **grid;
int **grid_lock, **grid_lock_sneaky;
int size_c, size_r;
char *line;
string line_str;
pthread_mutex_t mutexprivate, mutex_sneaky, mutex_sneaky_, mutexobey, mutex_active;
pthread_cond_t obey_cond;
vector<sem_t> sem_requests;
vector<request_order> request_orders;
vector<sem_t> sem_responses;
vector<bool> alive;
int current_obey = -1;
timespec current_obey_time;
bool flag_obey = false;
int order_index;
int active_thread_count = 0;
timeval start_time;

bool is_it_time_yet(timespec wait_till){
    timeval v;
    gettimeofday(&v, NULL);
    timespec now;
    now.tv_sec = v.tv_sec;
    now.tv_nsec = v.tv_usec * 1000;
    if (now.tv_sec > wait_till.tv_sec)
        return true;
    if (now.tv_sec == wait_till.tv_sec)
    {
        if (now.tv_nsec > wait_till.tv_nsec)
            return true;
    }
    return false;
}
timespec time_get_delayed(long delay_ms){
    timeval v;
    gettimeofday(&v, NULL);
    timespec now;
    now.tv_sec = v.tv_sec;
    now.tv_nsec = v.tv_usec * 1000;
    now.tv_nsec += delay_ms * 1000;
    while (now.tv_nsec >= 1000000000L)
    {
        now.tv_sec++;
        now.tv_nsec -= 1000000000L;
    }
    return now;
}
long how_many_us(timespec t){
    timeval v;
    gettimeofday(&v, NULL);
    timespec now;
    now.tv_sec = v.tv_sec;
    now.tv_nsec = v.tv_usec * 1000;
    long us = (-now.tv_sec + t.tv_sec) * 1000000 + (-now.tv_nsec + t.tv_nsec) / 1000;
    return abs(us);
}
void active_thread_count_minus(){
    pthread_mutex_lock(&mutex_active);
    active_thread_count--;
    pthread_mutex_unlock(&mutex_active);
}
void *commander_thread(void *arg)
{
    // 0 break
    // 1 contunie
    // 2 stop

    vector<obey> *obey_list = (vector<obey> *)arg;
    for (int i = 0; i < obey_list->size(); i++)
    {
        current_obey = (*obey_list)[i].obey;
        current_obey_time.tv_sec = start_time.tv_sec + (*obey_list)[i].time / 1000;
        current_obey_time.tv_nsec = (start_time.tv_usec + ((*obey_list)[i].time % 1000)*1000) * 1000;
        if (current_obey_time.tv_nsec >= 1000000000L)
        {
            current_obey_time.tv_sec++;
            current_obey_time.tv_nsec -= 1000000000L;
        }
        int succesful_threads = 0;
        vector<bool> got_order(sem_requests.size(), false);
        while (succesful_threads < active_thread_count)
        {
            vector<pthread_t> threads_to_join;
            vector<int> threads_to_join_index;
            vector<pthread_t> threads;
            threads.resize(active_thread_count);
            for (size_t i = 0; i < sem_requests.size(); i++)
            {
                if (alive[i])
                {
                    if (!sem_trywait(&sem_requests[i]))
                    {
                        pthread_create(&threads[i], NULL, timed_request<request_order>, (void *)&request_orders[i]);
                        threads_to_join.push_back(threads[i]);
                        threads_to_join_index.push_back(i);
                    }
                }
            }
            for (size_t i = 0; i < threads_to_join.size(); i++)
            {
                int *ret;
                pthread_join(threads_to_join[i], (void **)&ret);
                if (ret&& !got_order[threads_to_join_index[i]]){
                    got_order[threads_to_join_index[i]] = true;
                    succesful_threads++;
                }
            }
        }
        while(!is_it_time_yet(current_obey_time))
        {
            long us = how_many_us(current_obey_time);
            usleep(us);
        }
        // hw2 notify here
        if(current_obey == 2)
            {hw2_notify(ORDER_STOP, 0, 0, 0);
            }
        else if(current_obey == 1){
            hw2_notify(ORDER_CONTINUE, 0, 0, 0);
        }
        else if(current_obey == 0){
            hw2_notify(ORDER_BREAK, 0, 0, 0);
        }
        current_obey = -1;
        for (int i = 0; i < sem_requests.size(); i++)
        {
            if (alive[i])
            {
                if(sem_trywait(&sem_requests[i]))
                    sem_post(&sem_responses[i]);
                sem_post(&sem_responses[i]);
            }
        }
        pthread_yield();
    }
    current_obey = -1;
    pthread_t* threads_to_join = (pthread_t*)malloc(sizeof(pthread_t) * active_thread_count*2);
    pthread_t* threads = threads_to_join+active_thread_count;
    while (active_thread_count)
    {
        int j = 0;
        for (size_t i = 0; i < sem_requests.size(); i++)
        {
            if (alive[i])
            {
                if (!sem_trywait(&sem_requests[i]))
                {
                    pthread_create(&threads[i], NULL, timed_request_detached<request_order>, (void *)&request_orders[i]);
                    threads_to_join[j] = threads[i];
                    j++;
                }
            }
        }
    }
}
void unlock(int start_i, int stop_i, int start_j, int stop_j)
{
    for (int i = start_i; i < stop_i; i++)
    {
        for (int j = start_j; j < stop_j; j++)
        {
            grid_lock[i][j] = 0;
        }
    }
}

int unlocking_request(int id, long timeout_us, pthread_mutex_t *m)
{
    request_orders[id].timeout_us = timeout_us;
    request_orders[id].id = id;
    int obey;
    sem_post(&sem_requests[id]);
    obey = request_orders[id].obey;
    if (obey != -1 && obey != 1)
    {
        pthread_mutex_unlock(m);
        pthread_yield();
    }
    sem_wait(&sem_responses[id]);
    return obey;
}
int request(int id, long timeout_us)
{
    request_orders[id].timeout_us = timeout_us;
    request_orders[id].id = id;
    int obey;
    sem_post(&sem_requests[id]);
    sem_wait(&sem_responses[id]);
    obey = request_orders[id].obey;
    return obey;
}
int sneaky_recieve_order(int obey, int sid){
    if (obey == -1)
    {
        return -1;
    }
    switch (obey)
    {
    case 2:
        hw2_notify(SNEAKY_SMOKER_STOPPED, sid, 0, 0);
        active_thread_count_minus();
        pthread_exit(NULL);
        break;
    }
    return -1;
}
int sneaky_recieve_locked(int obey, int sid, pthread_mutex_t* m){
    if (obey == -1)
    {
        return -1;
    }
    switch (obey)
    {
    case 2:
        pthread_mutex_unlock(m);
        hw2_notify(SNEAKY_SMOKER_STOPPED, sid, 0, 0);
        active_thread_count_minus();
        pthread_exit(NULL);
        break;
    }
    return -1;
}
void *sneaky_smokers(void *arg)
{
    int obey = -1;
    bool flag = false;
    const sneaky *sneaky_smoker = (sneaky *)arg;
    hw2_notify(SNEAKY_SMOKER_CREATED, sneaky_smoker->sid, 0, 0);
    for (int m = 0; m < sneaky_smoker->cell_count; m++)
    {
    label2:
        obey = request(sneaky_smoker->id, 10);
        sneaky_recieve_order(obey, sneaky_smoker->sid);
        //pthread_mutex_lock(&mutexprivate);
        if(pthread_mutex_trylock(&mutexprivate)){
            goto label2;
        }
        for (int i = sneaky_smoker->sneaky_area[m][0] - 1; i < sneaky_smoker->sneaky_area[m][0] + 2; i++)
        {
            for (int j = sneaky_smoker->sneaky_area[m][1] - 1; j < sneaky_smoker->sneaky_area[m][1] + 2; j++)
            {
                obey = request(sneaky_smoker->id, 10);
                sneaky_recieve_locked(obey, sneaky_smoker->sid,&mutexprivate);
                if (grid_lock[i][j] == 1)
                {
                    pthread_mutex_unlock(&mutexprivate);
                    goto label2;
                }
            }
        }
        if (grid_lock_sneaky[sneaky_smoker->sneaky_area[m][0]][sneaky_smoker->sneaky_area[m][1]] == 1)
        {
            obey = request(sneaky_smoker->id, 10);
            sneaky_recieve_locked(obey, sneaky_smoker->sid,&mutexprivate);
            pthread_mutex_unlock(&mutexprivate);
            goto label2;
        }
        else
        {
            obey = request(sneaky_smoker->id, 10);
            sneaky_recieve_locked(obey, sneaky_smoker->sid,&mutexprivate);
            grid_lock_sneaky[sneaky_smoker->sneaky_area[m][0]][sneaky_smoker->sneaky_area[m][1]] = 1;
        }
        for (int i = sneaky_smoker->sneaky_area[m][0] - 1; i < sneaky_smoker->sneaky_area[m][0] + 2; i++)
        {
            for (int j = sneaky_smoker->sneaky_area[m][1] - 1; j < sneaky_smoker->sneaky_area[m][1] + 2; j++)
            {
                obey = request(sneaky_smoker->id, 10);
                sneaky_recieve_locked(obey, sneaky_smoker->sid,&mutexprivate);
                if (grid_lock[i][j] >= 2)
                {
                    grid_lock[i][j] = grid_lock[i][j] + 1;
                }
                else
                {
                    grid_lock[i][j] = 2;
                }
            }
        }
        hw2_notify(SNEAKY_SMOKER_ARRIVED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0], sneaky_smoker->sneaky_area[m][1]);
        pthread_mutex_unlock(&mutexprivate);
        pthread_yield();
        int temp_cigara = sneaky_smoker->sneaky_area[m][2];
        while (temp_cigara > 0)
        {
            if (temp_cigara > 0)
            {
                temp_cigara--; // 1
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0] - 1, sneaky_smoker->sneaky_area[m][1] - 1);
                grid[sneaky_smoker->sneaky_area[m][0] - 1][sneaky_smoker->sneaky_area[m][1] - 1] = grid[sneaky_smoker->sneaky_area[m][0] - 1][sneaky_smoker->sneaky_area[m][1] - 1] + 1;
            }
            if (temp_cigara > 0)
            {
                temp_cigara--; // 2
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0] - 1, sneaky_smoker->sneaky_area[m][1]);
                grid[sneaky_smoker->sneaky_area[m][0] - 1][sneaky_smoker->sneaky_area[m][1]] = grid[sneaky_smoker->sneaky_area[m][0] - 1][sneaky_smoker->sneaky_area[m][1]] + 1;
            }
            if (temp_cigara > 0)
            {
                temp_cigara--; // 3
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0] - 1, sneaky_smoker->sneaky_area[m][1] + 1);
                grid[sneaky_smoker->sneaky_area[m][0] - 1][sneaky_smoker->sneaky_area[m][1] + 1] = grid[sneaky_smoker->sneaky_area[m][0] - 1][sneaky_smoker->sneaky_area[m][1] + 1] + 1;
            }
            if (temp_cigara > 0)
            {
                temp_cigara--; // 4
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0], sneaky_smoker->sneaky_area[m][1] + 1);
                grid[sneaky_smoker->sneaky_area[m][0]][sneaky_smoker->sneaky_area[m][1] + 1] = grid[sneaky_smoker->sneaky_area[m][0]][sneaky_smoker->sneaky_area[m][1] + 1] + 1;
            }
            if (temp_cigara > 0)
            {
                temp_cigara--; // 5
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0] + 1, sneaky_smoker->sneaky_area[m][1] + 1);
                grid[sneaky_smoker->sneaky_area[m][0] + 1][sneaky_smoker->sneaky_area[m][1] + 1] = grid[sneaky_smoker->sneaky_area[m][0] + 1][sneaky_smoker->sneaky_area[m][1] + 1] + 1;
            }
            if (temp_cigara > 0)
            {
                temp_cigara--; // 6
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0] + 1, sneaky_smoker->sneaky_area[m][1]);
                grid[sneaky_smoker->sneaky_area[m][0] + 1][sneaky_smoker->sneaky_area[m][1]] = grid[sneaky_smoker->sneaky_area[m][0] + 1][sneaky_smoker->sneaky_area[m][1]] + 1;
            }
            if (temp_cigara > 0)
            {
                temp_cigara--; // 7
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0] + 1, sneaky_smoker->sneaky_area[m][1] - 1);
                grid[sneaky_smoker->sneaky_area[m][0] + 1][sneaky_smoker->sneaky_area[m][1] - 1] = grid[sneaky_smoker->sneaky_area[m][0] + 1][sneaky_smoker->sneaky_area[m][1] - 1] + 1;
            }
            if (temp_cigara > 0)
            {
                temp_cigara--; // 8
                timespec till_smoking_time = time_get_delayed(sneaky_smoker->time_to_smoke*1000);
                while(!is_it_time_yet(till_smoking_time)){
                    long us = how_many_us(till_smoking_time);
                    obey = request(sneaky_smoker->id, us);
                    sneaky_recieve_order(obey, sneaky_smoker->sid);
                }
                hw2_notify(SNEAKY_SMOKER_FLICKED, sneaky_smoker->sid, sneaky_smoker->sneaky_area[m][0], sneaky_smoker->sneaky_area[m][1] - 1);
                grid[sneaky_smoker->sneaky_area[m][0]][sneaky_smoker->sneaky_area[m][1] - 1] = grid[sneaky_smoker->sneaky_area[m][0]][sneaky_smoker->sneaky_area[m][1] - 1] + 1;
            }
        }
        hw2_notify(SNEAKY_SMOKER_LEFT, sneaky_smoker->sid, 0, 0);
        for (int i = sneaky_smoker->sneaky_area[m][0] - 1; i < sneaky_smoker->sneaky_area[m][0] + 2; i++)
        {
            for (int j = sneaky_smoker->sneaky_area[m][1] - 1; j < sneaky_smoker->sneaky_area[m][1] + 2; j++)
            {
                obey = request(sneaky_smoker->id, 10);
                sneaky_recieve_order(obey, sneaky_smoker->sid);
                if (grid_lock[i][j] > 2)
                {
                    grid_lock[i][j] = grid_lock[i][j] - 1;
                }
                else if (grid_lock[i][j] == 2)
                {
                    grid_lock[i][j] = 0;
                }
            }
        }
        obey = request(sneaky_smoker->id, 10);
        sneaky_recieve_order(obey, sneaky_smoker->sid);
        grid_lock_sneaky[sneaky_smoker->sneaky_area[m][0]][sneaky_smoker->sneaky_area[m][1]] = 0;
    }
    hw2_notify(SNEAKY_SMOKER_EXITED, sneaky_smoker->sid, 0, 0);
    pthread_mutex_lock(&mutex_active);
    active_thread_count--;
    pthread_mutex_unlock(&mutex_active);
    alive[sneaky_smoker->id] = false;
    pthread_exit(NULL);
}
void proper_waiting_to_continue(int gid, int id)
{
    int obey = -1;
    while (obey == -1)
    {
        obey = request(id, 500);
        switch (obey)
        {
        case 1:
            hw2_notify(PROPER_PRIVATE_CONTINUED, gid, 0, 0);
            break;
        case 2:
            hw2_notify(PROPER_PRIVATE_STOPPED, gid, 0, 0);
            active_thread_count_minus();
            pthread_exit(NULL);
            break;
        case 0:
            obey = -1;
            break;
        }
    }
}
int proper_recieve_order(int obey, int start_i, int stop_i, int start_j, int stop_j, int gid, pthread_mutex_t *m)
{
    if (obey == -1)
    {
        return -1;
    }
    switch (obey)
    {
    case 0:
        pthread_mutex_lock(m);
        unlock(start_i, stop_i, start_j, stop_j);
        pthread_mutex_unlock(m);
        hw2_notify(PROPER_PRIVATE_TOOK_BREAK, gid, 0, 0);
        return 0;
        break;
    case 2:
        hw2_notify(PROPER_PRIVATE_STOPPED, gid, 0, 0);
        active_thread_count_minus();
        pthread_exit(NULL);
        break;
    }
    return -1;
}
int waiting_proper_recieve_order(int obey, int gid)
{
    if (obey == -1)
    {
        return -1;
    }
    switch (obey)
    {
    case 0:
        hw2_notify(PROPER_PRIVATE_TOOK_BREAK, gid, 0, 0);
        return 0;
        break;
    case 2:
        hw2_notify(PROPER_PRIVATE_STOPPED, gid, 0, 0);
        active_thread_count_minus();
        pthread_exit(NULL);
        break;
    }
    return -1;
}

void *proper_cleaning(void *arg)
{
    proper *p = (proper *)arg;
    hw2_notify(PROPER_PRIVATE_CREATED, p->gid, 0, 0);
    int obey = -1;
    for (int m = 0; m < p->n_g; m++)
    {
    label:
        bool lock = true;
        obey = request(p->id, 10);
        if (waiting_proper_recieve_order(obey, p->gid) == 0)
        {
            proper_waiting_to_continue(p->gid, p->id);
            goto label;
        }
        for (int i = p->areas[m][0]; (i < p->s_i + p->areas[m][0]); i++)
        {
            for (int j = p->areas[m][1]; (j < p->s_j + p->areas[m][1]); j++)
            {
                obey = request(p->id, 10);
                if (waiting_proper_recieve_order(obey, p->gid) == 0)
                {
                    proper_waiting_to_continue(p->gid, p->id);
                    goto label;
                }
                if (grid_lock[i][j] != 0)
                {
                    goto label;
                }
            }
        }
        if(pthread_mutex_trylock(&mutexprivate)){
            goto label;
        }
         obey = unlocking_request(p->id, 10, &mutexprivate);
                if (waiting_proper_recieve_order(obey, p->gid) == 0)
                {
                    proper_waiting_to_continue(p->gid, p->id);
                    goto label;
                }
        for (int i = p->areas[m][0]; (i < p->s_i + p->areas[m][0]); i++)
        {
            for (int j = p->areas[m][1]; (j < p->s_j + p->areas[m][1]); j++)
            {
                obey = unlocking_request(p->id, 10, &mutexprivate);
                if (waiting_proper_recieve_order(obey, p->gid) == 0)
                {
                    proper_waiting_to_continue(p->gid, p->id);
                    goto label;
                }
                if (grid_lock[i][j] != 0)
                {
                    lock = false;
                    pthread_mutex_unlock(&mutexprivate);
                    goto label;
                }
            }
        }
        for (int i = p->areas[m][0]; (i < p->s_i + p->areas[m][0]); i++)
        {
            for (int j = p->areas[m][1]; (j < p->s_j + p->areas[m][1]); j++)
            {
                obey = unlocking_request(p->id, 10, &mutexprivate);
                if (proper_recieve_order(obey, p->areas[m][0], p->areas[m][0] + p->s_i, p->areas[m][1], p->areas[m][1] + p->s_j, p->gid, &mutexprivate) == 0)
                {
                    proper_waiting_to_continue(p->gid, p->id);
                    goto label;
                }
                grid_lock[i][j] = 1;
            }
        }
        pthread_mutex_unlock(&mutexprivate);
        obey = request(p->id, 10);
        if (proper_recieve_order(obey, p->areas[m][0], p->areas[m][0] + p->s_i, p->areas[m][1], p->areas[m][1] + p->s_j, p->gid, &mutexprivate) == 0)
        {
            proper_waiting_to_continue(p->gid, p->id);
            goto label;
        }
        hw2_notify(PROPER_PRIVATE_ARRIVED, p->gid, p->areas[m][0], p->areas[m][1]);
        obey = request(p->id, 10);
                if (proper_recieve_order(obey, p->areas[m][0], p->areas[m][0] + p->s_i, p->areas[m][1], p->areas[m][1] + p->s_j, p->gid, &mutexprivate) == 0)
                {
                    proper_waiting_to_continue(p->gid, p->id);
                    goto label;
                }
        for (int i = p->areas[m][0]; (i < p->s_i + p->areas[m][0]); i++)
        {

            for (int j = p->areas[m][1]; (j < p->s_j + p->areas[m][1]); j++)
            {
                obey = request(p->id, 10);
                if (proper_recieve_order(obey, p->areas[m][0], p->areas[m][0] + p->s_i, p->areas[m][1], p->areas[m][1] + p->s_j, p->gid, &mutexprivate) == 0)
                {
                    proper_waiting_to_continue(p->gid, p->id);
                    goto label;
                }
                while (grid[i][j])
                {
                    // usleep((p->t_g) * 1000);
                    timespec time_till_gather = time_get_delayed(p->t_g*1000);
                    while(!is_it_time_yet(time_till_gather)){
                        long us = how_many_us(time_till_gather);
                        obey = request(p->id, us);
                        if (proper_recieve_order(obey, p->areas[m][0], p->areas[m][0] + p->s_i, p->areas[m][1], p->areas[m][1] + p->s_j, p->gid, &mutexprivate) == 0)
                        {
                            proper_waiting_to_continue(p->gid, p->id);
                            goto label;
                        }
                    }
                    grid[i][j]--;
                    hw2_notify(PROPER_PRIVATE_GATHERED, p->gid, i, j);
                }
            }
        }
        hw2_notify(PROPER_PRIVATE_CLEARED, p->gid, 0, 0);
        for (int i = p->areas[m][0]; (i < p->s_i + p->areas[m][0]); i++)
        {

            for (int j = p->areas[m][1]; (j < p->s_j + p->areas[m][1]); j++)
            {
                obey = request(p->id, 10);
                if (proper_recieve_order(obey, p->areas[m][0], p->areas[m][0] + p->s_i, p->areas[m][1], p->areas[m][1] + p->s_j, p->gid, &mutexprivate) == 0)
                {
                    proper_waiting_to_continue(p->gid, p->id);
                    i = p->s_i + p->areas[m][0];
                    break;
                }
                grid_lock[i][j] = 0;
            }
        }
    }
    hw2_notify(PROPER_PRIVATE_EXITED, p->gid, 0, 0);
    pthread_mutex_lock(&mutex_active);
    active_thread_count--;
    pthread_mutex_unlock(&mutex_active);
    alive[p->id] = false;
    pthread_exit(NULL);
}

int main()
{

    int number_of_proper, number_of_sneaky, number_of_obey;
    cin >> size_c >> size_r;
    number_of_sneaky = 0;
    number_of_obey = 0;
    vector<proper> proper_list;
    grid = new int *[size_c];
    grid_lock = new int *[size_c];
    grid_lock_sneaky = new int *[size_c];
    for (int i = 0; i < size_c; ++i)
    {
        grid[i] = new int[size_r];
        grid_lock[i] = new int[size_r];
        grid_lock_sneaky[i] = new int[size_r];
    }

    for (int i = 0; i < size_c; i++)
    {

        for (int j = 0; j < size_r; j++)
        {
            int temp;
            cin >> temp;
            grid[i][j] = temp;

            grid_lock[i][j] = 0;
            grid_lock_sneaky[i][j] = 0;
        }
    }
    cin >> number_of_proper;
    for (int i = 0; i < number_of_proper; i++)
    {
        int gid, s_i, s_j, t_g, n_g;
        cin >> gid >> s_i >> s_j >> t_g >> n_g;
        proper p;
        p.gid = gid;
        p.s_i = s_i;
        p.s_j = s_j;
        p.t_g = t_g;
        p.n_g = n_g;
        p.id = i;
        alive.push_back(1);
        sem_responses.emplace_back();
        sem_init(&sem_responses[i], 0, 0);
        sem_requests.emplace_back();
        sem_init(&sem_requests[i], 0, 0);
        request_orders.emplace_back();
        request_orders[i].id = i;
        for (int j = 0; j < n_g; j++)
        {
            int k1, k2;
            cin >> k1 >> k2;
            p.areas.push_back({k1, k2});
        }
        proper_list.push_back(p);
    }

    cin >> number_of_obey;
    vector<obey> obey_list;
    for (int i = 0; i < number_of_obey; i++)
    {
        int time;
        char name[9];
        cin >> time >> name;
        obey b;
        // 0 break
        // 1 contunie
        // 2 stop
        if (!strcmp(name, "break"))
        {
            b.time = time;
            b.obey = 0;
        }
        else if (!strcmp(name, "stop"))
        {
            b.time = time;
            b.obey = 2;
        }
        else if (!strcmp(name, "continue"))
        {
            b.time = time;
            b.obey = 1;
        }

        obey_list.push_back(b);
    }
    cin >> number_of_sneaky;
    vector<sneaky> sneaky_list;
    for (int i = 0; i < number_of_sneaky; i++)
    {
        int sid, time_to_smoke, cell_count;
        cin >> sid >> time_to_smoke >> cell_count;
        sneaky s;
        s.sid = sid;
        s.time_to_smoke = time_to_smoke;
        s.cell_count = cell_count;
        s.id = i + number_of_proper;
        for (int j = 0; j < cell_count; j++)
        {
            int k1, k2, k3;
            cin >> k1 >> k2 >> k3;
            s.sneaky_area.push_back({k1, k2, k3});
        }
        alive.push_back(1);
        sem_responses.emplace_back();
        sem_init(&sem_responses[i + number_of_proper], 0, 0);
        sem_requests.emplace_back();
        sem_init(&sem_requests[i + number_of_proper], 0, 0);
        request_orders.emplace_back();
        request_orders[i + number_of_proper].id = i + number_of_proper;
        sneaky_list.push_back(s);
    }
    pthread_t thread[number_of_proper];
    pthread_mutex_init(&mutexprivate, NULL);
    pthread_mutex_init(&mutex_active, NULL);
    hw2_init_notifier();
    gettimeofday(&start_time, NULL);
    active_thread_count = number_of_proper + number_of_sneaky;
    
    for (int i = 0; i < number_of_proper; i++)
    {
        if (pthread_create(&thread[i], NULL, &proper_cleaning, (void *)&proper_list[i]))
        {
            cerr << "error creating thread" << endl;
        }
    }
    pthread_t thread_sneaky[number_of_sneaky];
    pthread_mutex_init(&mutex_sneaky, NULL);
    for (int i = 0; i < number_of_sneaky; i++)
    {
        if (pthread_create(&thread_sneaky[i], NULL, &sneaky_smokers, (void *)&sneaky_list[i]))
        {
            cerr << "error creating thread" << endl;
        }
    }
    pthread_t commander;
    if (pthread_create(&commander, NULL, &commander_thread, (void *)&obey_list))
    {
        cerr << "error creating thread" << endl;
    }
    for (int i = 0; i < number_of_proper; i++)
    {
        if (pthread_join(thread[i], NULL))
        {
            cerr << "Error joining thread" << endl;
            return 1;
        }
    }
    for (int i = 0; i < number_of_sneaky; i++)
    {
        if (pthread_join(thread_sneaky[i], NULL))
        {
            cerr << "error joining thread" << endl;
            return 1;
        }
    }
    if (pthread_join(commander, NULL))
    {
        cerr << "error joining thread" << endl;
        return 1;
    }
}