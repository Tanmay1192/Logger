#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define TL_BUFFER_SIZE     100 // 64 Kb
#define MAX_VA_MSG_SIZE    80  //  4 Kb
#define NUM_BUFF_PER_THREAD 2

typedef struct ThreadBufferT
{
    unsigned short size;
    char status;
    char* data;
}ThreadBuffer_t;

typedef struct ThreadMapT
{
    ThreadBuffer_t *tbuf[NUM_BUFF_PER_THREAD];
}ThreadMap_t;


static pthread_mutex_t thread_map_mutex;

// global counter for thread indexing
static int thread_index_g = -1;

// Map for thread index and their 2 buffer
static ThreadMap_t *thread_map = NULL;


// Thread specific index
static __thread int thread_index = -1;
static __thread ThreadBuffer_t t_buf[NUM_BUFF_PER_THREAD];
static void gen_thread_data()
{
  if(thread_index == -1)
  {
    /*Start Lock*/
    pthread_mutex_lock(&thread_map_mutex);
    thread_index_g++;
    thread_index = thread_index_g;

    //TODO: allocate memory by Delta size (say 5 or 10)
    thread_map = (ThreadMap_t *)realloc(thread_map, (thread_index_g+1) * sizeof(ThreadMap_t));
    pthread_mutex_unlock(&thread_map_mutex);
    /*End Lock*/

    t_buf[0].data  = (char*)calloc(1, TL_BUFFER_SIZE);
    t_buf[0].status = 0;

    t_buf[1].data = (char*)calloc(1, TL_BUFFER_SIZE);
    t_buf[1].status = 0;

    thread_map[thread_index].tbuf[0] = &t_buf[0];
    thread_map[thread_index].tbuf[1] = &t_buf[1];
  }
}

int print_data(const char* format, ...)
{
    // Thread specific static variable
    static __thread int cur_buf_index = 0;
    static __thread int va_string_size = 0;
    static __thread char va_msg[MAX_VA_MSG_SIZE];
    static char *ll_operation_name[6]={"CRICTICAL", "ERROR", "WARNING", "DEBUG", "INFO", "STRACE"};

    if(thread_index == -1)
    {
        gen_thread_data();
    }

    char cur_time[20 + 1] = "";
    va_list args;

    // dummy
    int pid = 123456;
    strcpy(cur_time,"HH:MM:SS");
    char *func_name = "func()";
    int line = 100;
    int level = 2;

    va_start( args, format );

    // partial handling
    va_string_size = snprintf(va_msg, MAX_VA_MSG_SIZE,
            "%s |%d |%s |%s : %d |",
            cur_time, pid, ll_operation_name[level], func_name, line);

    va_string_size += vsnprintf(va_msg + va_string_size, (MAX_VA_MSG_SIZE - va_string_size), format, args);
    va_msg[va_string_size++] = '\n';

    if(( t_buf[cur_buf_index].size + va_string_size ) >= TL_BUFFER_SIZE)
    {
        t_buf[cur_buf_index].status = 1;

        if(cur_buf_index == 0)
        {
            cur_buf_index = 1;
        }
        else
        {
            cur_buf_index = 0;
        }
    }
    if(t_buf[cur_buf_index].status == 0)
    {
        int size = t_buf[cur_buf_index].size;
        memcpy(&t_buf[cur_buf_index].data[size], va_msg, va_string_size);
        t_buf[cur_buf_index].size += va_string_size;
    }
    else
    {
        printf("Buffer is full t_buf[%d].data = [%s]", cur_buf_index, t_buf[cur_buf_index].data);
        // TODO - Both Buffer are full so store the log in temp buffer
    }

    /* free args */
    va_end( args );
    return 0;
}


void flushThreadMap()
{
    size_t data_len = 0;
    int i;

    for(i = 0; i < thread_index_g; ++i)
    {
        //memset data
        if(thread_map[i].tbuf[0]->data[0] != '\0')
        {
            data_len = strlen(thread_map[i].tbuf[0]->data);

            fprintf(stderr, "%s",  thread_map[i].tbuf[0]->data);

            memset(thread_map[i].tbuf[0]->data, 0, data_len);
            thread_map[i].tbuf[0]->size = 0;
            thread_map[i].tbuf[0]->status = 0;
        }

        if(thread_map[i].tbuf[1]->data[0] != '\0')
        {
            data_len = strlen(thread_map[i].tbuf[1]->data);

            fprintf(stderr, "%s",  thread_map[i].tbuf[1]->data);

            memset(thread_map[i].tbuf[1]->data, 0, data_len);
            thread_map[i].tbuf[1]->size = 0;
            thread_map[i].tbuf[1]->status = 0;
        }
    }
}
void m_thread()
{
    int i;
    size_t data_len = 0;
    while(1)
    {
        for(i = 0; i < thread_index_g; ++i)
        {
            if(thread_map[i].tbuf[0]->status == 1)
            {
                data_len = strlen(thread_map[i].tbuf[0]->data);

                fprintf(stderr, "%s",  thread_map[i].tbuf[0]->data);

                memset(thread_map[i].tbuf[0]->data, 0, data_len);
                thread_map[i].tbuf[0]->size = 0;
                thread_map[i].tbuf[0]->status = 0;
            }
            if(thread_map[i].tbuf[1]->status == 1)
            {
                data_len = strlen(thread_map[i].tbuf[1]->data);

                fprintf(stderr, "%s",  thread_map[i].tbuf[1]->data);

                memset(thread_map[i].tbuf[1]->data, 0, data_len);
                thread_map[i].tbuf[1]->size = 0;
                thread_map[i].tbuf[1]->status = 0;
            }
        }
        sleep(1);
    }
}

void display()
{
    int i;
    printf("Method Entry %s\n", __func__);
    for(i = 0; i < 200; ++i )
    {
        print_data("Hello World!!; This is line number %d", i);
    }
    printf("Method Exit %s\n", __func__);
}

void init_logger()
{
    pthread_t m_tid;
    pthread_mutex_init(&thread_map_mutex, NULL);

    pthread_create(&m_tid, NULL, (void *)&m_thread, NULL);

    atexit(flushThreadMap);
}

//test main
int main(void)
{

    init_logger();
    display();
    return 0;
}
