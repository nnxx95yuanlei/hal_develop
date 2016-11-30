#include "sys/types.h"
#include "poll.h"
#include "pthread.h"
#include "fcntl.h"
#include "string.h"
#include "errno.h"
#include "stdlib.h"


#define PIPE_READ 0
#define PIPE_WRITE 1
#define MAX_FD_IN_POLL 10

//////////////////compile support/////////
#ifdef _ANDROID_

#else
#define TRUE     1
#define FALSE    0
#define THREAD_NAME_SIZE    32

typedef unsigned int  uint32_t;

#include "stdio.h"
#define CC_ERROR(fmt,args...)  fprintf(stderr, fmt, ##args)
#define CC_WARNING(fmt,args...)  fprintf(stderr, fmt, ##args)
#define CC_DEBUG(fmt,args...)  fprintf(stderr, fmt, ##args)
#endif
////////////

typedef void (*cc_poll_notify)(void *user_data);

typedef enum {
  CC_POLL_TYPE_EVENT = 0x00,
  CC_POLL_TYPE_DATA,
}cc_poll_type_t;

typedef enum {
//  CC_POLL_THREAD_PIPE_UPDATE = 0x00,
  CC_POLL_THREAD_PIPE_ADD_FD = 0x00,
  CC_POLL_THREAD_PIPE_DEL_FD,
  CC_POLL_THREAD_EXIT,
  CC_POLL_THREAD_CMD_MAX,
}cc_poll_thread_cmd_t;

typedef enum {
  CC_POLL_THREAD_STATE_STOPED = 0x00,
  CC_POLL_THREAD_STATE_POLLING,
  CC_POLL_THREAD_STATE_MAX
}cc_poll_thread_state_t;


typedef struct {
    int32_t fd;
    cc_poll_notify notify_cb;
    void* user_data;
} cc_poll_notify_entry_t;

typedef struct {
    cc_poll_notify_entry_t poll_entries;
//  uint8_t num_entries;
}cc_pipe_event_data_t;

typedef struct {
  cc_poll_thread_cmd_t cmd;
  cc_pipe_event_data_t data;
  int32_t sync;
}cc_pipe_event_t;

typedef struct {
    char threadName[THREAD_NAME_SIZE];
    struct pollfd poll_fds[MAX_FD_IN_POLL + 1];
    int32_t num_fds;
    cc_poll_notify_entry_t poll_entries[MAX_FD_IN_POLL];
    int32_t num_entries;
    pthread_t pid;
}cc_poll_thread_dump_t;

typedef struct {
    cc_poll_type_t poll_type;
    cc_poll_notify_entry_t poll_entries[MAX_FD_IN_POLL];
    int32_t num_entries;
    int32_t pfds[2];
    pthread_t pid;
    cc_poll_thread_state_t state;
    int timeoutms;
//    cc_poll_thread_cmd_t cmd;
    struct pollfd poll_fds[MAX_FD_IN_POLL + 1];   //+1 means we need to add pipe read fd in poll_fds[0] position
    int32_t num_fds;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char threadName[THREAD_NAME_SIZE];
    uint32_t ready; // whether work thread has been ready ?
    void* poll_private;
} cc_poll_thread_t;




int32_t cc_poll_thread_launch(cc_poll_thread_t** poll_handle,
                                     cc_poll_type_t poll_type,
                                     char* thread_name);

int32_t cc_poll_thread_add_fd(cc_poll_thread_t* poll_th,
                            int32_t fd,
                            cc_poll_notify notify_cb,
                            void* userdata,
                            int32_t sync);

int32_t cc_poll_thread_del_fd(cc_poll_thread_t* poll_th,
                            int32_t fd,
                            int32_t sync);

int32_t cc_poll_thread_release(cc_poll_thread_t** poll_handle);


int32_t cc_poll_thread_dump(cc_poll_thread_t* poll_th,
                                  cc_poll_thread_dump_t* dump);




