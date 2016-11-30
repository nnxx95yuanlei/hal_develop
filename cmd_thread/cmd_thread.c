
#include "cmd_thread.h"


static void cc_poll_thread_wait_for_ready(cc_poll_thread_t* poll_th);


int32_t cc_poll_thread_add_fd(cc_poll_thread_t* poll_th,
                                          int32_t fd,
                                          cc_poll_notify notify_cb,
                                          void* userdata,
                                          int32_t sync)
{
    int32_t rc = 0;
    cc_pipe_event_t pipe_evt;

    memset(&pipe_evt, 0, sizeof(pipe_evt));
    pipe_evt.cmd = CC_POLL_THREAD_PIPE_ADD_FD;
    pipe_evt.data.poll_entries.fd = fd;
    pipe_evt.data.poll_entries.notify_cb = notify_cb;
    pipe_evt.data.poll_entries.user_data = userdata;
    pipe_evt.sync = sync;

    pthread_mutex_lock(&poll_th->mutex);
    poll_th->ready = FALSE;

    ssize_t len = write(poll_th->pfds[PIPE_WRITE], &pipe_evt, sizeof(pipe_evt));
    if(len < 1) {
        CC_ERROR("%s: len = %lld, errno = %d\n", __func__,(long long int)len, errno);
        pthread_mutex_unlock(&poll_th->mutex);
        return -1;
    }
    pthread_mutex_unlock(&poll_th->mutex);

    if(sync)
        cc_poll_thread_wait_for_ready(poll_th);

    CC_DEBUG("%s: X\n", __func__);

    return rc;
}


int32_t cc_poll_thread_del_fd(cc_poll_thread_t* poll_th,
                                          int32_t fd,
                                          int32_t sync)
{
    int32_t rc = 0;
    cc_pipe_event_t pipe_evt;

    memset(&pipe_evt, 0, sizeof(pipe_evt));
    pipe_evt.cmd = CC_POLL_THREAD_PIPE_DEL_FD;
    pipe_evt.data.poll_entries.fd = fd;
    pipe_evt.sync = sync;

    pthread_mutex_lock(&poll_th->mutex);
    poll_th->ready = FALSE;

    ssize_t len = write(poll_th->pfds[PIPE_WRITE], &pipe_evt, sizeof(pipe_evt));
    if(len < 1) {
        CC_ERROR("%s: len = %lld, errno = %d\n", __func__,(long long int)len, errno);
        pthread_mutex_unlock(&poll_th->mutex);
        return -1;
    }
    pthread_mutex_unlock(&poll_th->mutex);

    if(sync)
        cc_poll_thread_wait_for_ready(poll_th);

    CC_DEBUG("%s: X\n", __func__);

    return rc;
}

int32_t cc_poll_thread_release(cc_poll_thread_t** poll_handle)
{
    int32_t rc = 0;
    cc_poll_thread_t* poll_th = (cc_poll_thread_t*)*poll_handle;
    if(NULL == poll_th) {
        CC_ERROR("%s null poll handle\n",__func__);
        return -1;
    }
    if(CC_POLL_THREAD_STATE_STOPED == poll_th->state) {
        CC_ERROR("%s: err, poll thread is not running.\n", __func__);
        return rc;
    }

    cc_pipe_event_t pipe_evt;
    memset(&pipe_evt, 0, sizeof(pipe_evt));
    pipe_evt.cmd = CC_POLL_THREAD_EXIT;
    pipe_evt.sync = TRUE;

    pthread_mutex_lock(&poll_th->mutex);
    poll_th->ready = FALSE;

    ssize_t len = write(poll_th->pfds[PIPE_WRITE], &pipe_evt, sizeof(pipe_evt));
    if(len < 1) {
        CC_ERROR("%s: len = %lld, errno = %d\n", __func__,(long long int)len, errno);
        pthread_mutex_unlock(&poll_th->mutex);
        return -1;
    }
    pthread_mutex_unlock(&poll_th->mutex);

    cc_poll_thread_wait_for_ready(poll_th);

    if (pthread_join(poll_th->pid, NULL) != 0) {
        CC_ERROR("%s: pthread dead already\n", __func__);
    }

    /* close pipe */
    if(poll_th->pfds[PIPE_READ] >= 0) {
        close(poll_th->pfds[PIPE_READ]);
    }
    if(poll_th->pfds[PIPE_WRITE] >= 0) {
        close(poll_th->pfds[PIPE_WRITE]);
    }

    pthread_mutex_destroy(&poll_th->mutex);
    pthread_cond_destroy(&poll_th->cond);

    free(poll_th);
    poll_th = NULL;
    *poll_handle = NULL;

    return rc;
}


static void cc_poll_thread_set_state(cc_poll_thread_t* poll_th, cc_poll_thread_state_t state) {
    pthread_mutex_lock(&poll_th->mutex);
    poll_th->state = state;
    pthread_mutex_unlock(&poll_th->mutex);
    return;
}

static void cc_poll_thread_wait_for_ready(cc_poll_thread_t* poll_th) {
    pthread_mutex_lock(&poll_th->mutex);
    if(!poll_th->ready) {
        pthread_cond_wait(&poll_th->cond, &poll_th->mutex);
    }
    pthread_mutex_unlock(&poll_th->mutex);
    return;
}

static void cc_poll_thread_singal_ready(cc_poll_thread_t* poll_th) {
  // signal to main thread to go on
  pthread_mutex_lock(&poll_th->mutex);
  poll_th->ready = TRUE;
  pthread_cond_signal(&poll_th->cond);
  pthread_mutex_unlock(&poll_th->mutex);
  CC_DEBUG("%s: ready, in mutex\n", __func__);
  return;
}

static void cc_poll_thread_proc_pipe_event(cc_poll_thread_t* poll_th) {
    ssize_t read_len;
    int i,j;
    cc_pipe_event_t pipe_evt;
    read_len = read(poll_th->pfds[PIPE_READ], &pipe_evt, sizeof(pipe_evt));
    CC_DEBUG("%s: read_fd = %d, read_len = %d, expect_len = %d cmd = %d\n",
         __func__, poll_th->pfds[PIPE_READ], (int)read_len, (int)sizeof(pipe_evt), pipe_evt.cmd);
    switch (pipe_evt.cmd) {
    case CC_POLL_THREAD_PIPE_ADD_FD:
    {
        cc_pipe_event_data_t* data = &(pipe_evt.data);

        //find whether fd has exsited in entries
        if(poll_th->num_entries >= MAX_FD_IN_POLL) {
            CC_ERROR("poll fds has reached max value(%d), add action failed\n",MAX_FD_IN_POLL);
            break;
        }

        if(poll_th->poll_type != CC_POLL_TYPE_EVENT && poll_th->poll_type != CC_POLL_TYPE_DATA) {
            CC_ERROR("poll type is invalid\n");
            break;
        }

        for(i = 0 ; i < poll_th->num_entries; i ++) {
            if(poll_th->poll_entries[i].fd == data->poll_entries.fd )
                break;
        }

        if(i == poll_th->num_entries) {
            if(data->poll_entries.fd >= 0) {
                poll_th->poll_entries[poll_th->num_entries].fd = data->poll_entries.fd;
                poll_th->poll_entries[poll_th->num_entries].notify_cb= data->poll_entries.notify_cb;
                poll_th->poll_entries[poll_th->num_entries].user_data= data->poll_entries.user_data;
                poll_th->num_entries++;
                poll_th->poll_fds[poll_th->num_fds].fd = data->poll_entries.fd;
                poll_th->poll_fds[poll_th->num_fds].events = POLLIN|POLLRDNORM|POLLPRI;
                poll_th->num_fds++;
            }
        } else {
            CC_ERROR("fd has already exsited in poll entries, have no need to add again\n");
        }
    }
        break;
    case CC_POLL_THREAD_PIPE_DEL_FD:
    {
        cc_pipe_event_data_t* data = &(pipe_evt.data);

        //find whether fd has exsited in entries
        if(poll_th->num_entries <= 0) {
            CC_ERROR("there are no fds in polling, del action failed\n");
            break;
        }

        if(poll_th->poll_type != CC_POLL_TYPE_EVENT && poll_th->poll_type != CC_POLL_TYPE_DATA) {
            CC_ERROR("poll type is invalid\n");
            break;
        }

        for(i = 0 ; i < poll_th->num_entries; i ++) {
            if(poll_th->poll_entries[i].fd == data->poll_entries.fd )
                break;
        }

        if(i == poll_th->num_entries) {
            CC_ERROR("fd has not exsited in poll entries, have no need to del, bypass\n");
        } else {
            if(i == MAX_FD_IN_POLL - 1) {
                poll_th->poll_entries[i].fd = -1;
                poll_th->poll_entries[i].notify_cb = NULL;
                poll_th->poll_entries[i].user_data = NULL;
                poll_th->num_entries--;
                poll_th->poll_fds[i + 1].fd = -1;
                poll_th->num_fds--;
            } else {
                for(j = i; j <poll_th->num_entries ; j++) {
                    poll_th->poll_entries[j].fd = poll_th->poll_entries[j+1].fd;
                    poll_th->poll_entries[j].notify_cb= poll_th->poll_entries[j+1].notify_cb;
                    poll_th->poll_entries[j].user_data= poll_th->poll_entries[j+1].user_data;
                    poll_th->poll_fds[j+1].fd = poll_th->poll_fds[j+2].fd;
                }
                poll_th->num_entries--;
                poll_th->num_fds--;
                poll_th->poll_entries[poll_th->num_entries].fd = -1;
                poll_th->poll_entries[poll_th->num_entries].notify_cb = NULL;
                poll_th->poll_entries[poll_th->num_entries].user_data = NULL;
                poll_th->poll_fds[poll_th->num_fds].fd = -1;
            }
        }
    }
        break;
    case CC_POLL_THREAD_EXIT:
    default:
        cc_poll_thread_set_state(poll_th, CC_POLL_THREAD_STATE_STOPED);
        break;
    }

    if(pipe_evt.sync)
        cc_poll_thread_singal_ready(poll_th);
    return;
}

static void cc_poll_thread_proc_fd_event(cc_poll_thread_t* poll_th) {
    int32_t i;

    if(NULL == poll_th)
        return;

    for(i=1; i<poll_th->num_fds; i++) {
        /* Checking events from fd */
        if ((poll_th->poll_type == CC_POLL_TYPE_EVENT) &&
            (poll_th->poll_fds[i].revents & POLLPRI)) {
            CC_DEBUG("%s: poll event in fd:%d , send to event notify\n", __func__,poll_th->poll_fds[i].fd);
            if (NULL != poll_th->poll_entries[i-1].notify_cb) {
                poll_th->poll_entries[i-1].notify_cb(poll_th->poll_entries[i-1].user_data);
            } else {
                CC_WARNING("there is no notify callback for fd:%d\n",poll_th->poll_fds[i].fd);
            }
        }
        /* Checking data from fd */
        if ((poll_th->poll_type == CC_POLL_TYPE_DATA) &&
            (poll_th->poll_fds[i].revents & POLLIN) &&
            (poll_th->poll_fds[i].revents & POLLRDNORM)) {
            CC_DEBUG("%s: poll data in fd:%d , send to data notify\n", __func__,poll_th->poll_fds[i].fd);
            if (NULL != poll_th->poll_entries[i-1].notify_cb) {
                poll_th->poll_entries[i-1].notify_cb(poll_th->poll_entries[i-1].user_data);
            } else {
                CC_WARNING("there is no notify callback for fd:%d\n",poll_th->poll_fds[i].fd);
            }
        }
    }

}




static void* __cc_poll_thread(void* userdata) {
    cc_poll_thread_t* poll_th = (cc_poll_thread_t*)userdata;
    int32_t rc,i;
    if(NULL == poll_th) {
      CC_ERROR("%s:%d: Exit poll thread, because of null pointer\n",__func__,__LINE__);
      return NULL;
    }

    //set poll_fds[0] for pipe
    poll_th->num_fds = 0;
    poll_th->poll_fds[poll_th->num_fds++].fd = poll_th->pfds[PIPE_READ];

    cc_poll_thread_set_state(poll_th,CC_POLL_THREAD_STATE_POLLING);
    cc_poll_thread_singal_ready(poll_th);

    CC_DEBUG("%s: poll type = %d, num_fd = %d poll_th = %p\n",
    __func__, poll_th->poll_type, poll_th->num_fds,poll_th);
    do {
         for(i = 0; i < poll_th->num_fds; i++) {
            poll_th->poll_fds[i].events = POLLIN|POLLRDNORM|POLLPRI;
         }

         rc = poll(poll_th->poll_fds, poll_th->num_fds, poll_th->timeoutms);
         if(rc > 0) {
            if ((poll_th->poll_fds[0].revents & POLLIN) &&
                (poll_th->poll_fds[0].revents & POLLRDNORM)) {
                CC_DEBUG("%s: cmd received on pipe\n", __func__);
                cc_poll_thread_proc_pipe_event(poll_th);
            } else {
                CC_DEBUG("%s: cmd received on fd\n", __func__);
                cc_poll_thread_proc_fd_event(poll_th);
            }
        } else {
            /* in error case sleep 10 us and then continue. hard coded here */
            CC_ERROR("polling error , wait 10 us and poll again!!!\n");
            usleep(10);
            continue;
        }
    } while ((poll_th != NULL) && (poll_th->state == CC_POLL_THREAD_STATE_POLLING));
    return NULL;

}

int32_t cc_poll_thread_launch(cc_poll_thread_t** poll_handle,
                                     cc_poll_type_t poll_type,
                                     char* thread_name)
{
    int32_t rc = 0;
    size_t i = 0, cnt = 0;
    cc_poll_thread_t* poll_th = (cc_poll_thread_t*)*poll_handle;
    if(NULL == poll_th) {
       poll_th = (cc_poll_thread_t*)malloc(sizeof(cc_poll_thread_t));
       if(NULL == poll_th) {
           CC_ERROR("no memory\n");
           return -1;
       }
       *poll_handle = poll_th;
    }

    memset(poll_th,0x00,sizeof(cc_poll_thread_t));

    poll_th->poll_type = poll_type;

    //Initialize poll_fds
    cnt = sizeof(poll_th->poll_fds) / sizeof(poll_th->poll_fds[0]);
    for (i = 0; i < cnt; i++) {
        poll_th->poll_fds[i].fd = -1;
    }
    //Initialize poll_entries
    cnt = sizeof(poll_th->poll_entries) / sizeof(poll_th->poll_entries[0]);
    for (i = 0; i < cnt; i++) {
        poll_th->poll_entries[i].fd = -1;
    }
    //Initialize thread name
    strcpy(poll_th->threadName,thread_name?thread_name:"cc_poll_thread");

    //Initialize pipe fds
    poll_th->pfds[0] = -1;
    poll_th->pfds[1] = -1;
    rc = pipe(poll_th->pfds);
    if(rc < 0) {
        CC_ERROR("%s: pipe open failed rc=%d\n", __func__, rc);
        return -1;
    }

    poll_th->timeoutms = -1;  /* Infinite seconds */

    CC_DEBUG("%s: poll_type = %d, read fd = %d, write fd = %d timeout = %d\n",
        __func__, poll_th->poll_type,
        poll_th->pfds[0], poll_th->pfds[1],poll_th->timeoutms);

    pthread_mutex_init(&poll_th->mutex, NULL);
    pthread_cond_init(&poll_th->cond, NULL);

    /* launch the thread */
    pthread_mutex_lock(&poll_th->mutex);
    poll_th->ready = FALSE;
    pthread_create(&poll_th->pid, NULL, __cc_poll_thread, (void *)poll_th);

    pthread_setname_np(poll_th->pid, poll_th->threadName);
    pthread_mutex_unlock(&poll_th->mutex);

    cc_poll_thread_wait_for_ready(poll_th);

    CC_DEBUG("%s: End\n",__func__);
    return rc;
}


int32_t cc_poll_thread_dump(cc_poll_thread_t* poll_th,
                                  cc_poll_thread_dump_t* dump) {
    int32_t i;
    if(poll_th == NULL || dump == NULL) {
        CC_ERROR("null pointer, cannot dump anything\n");
        return -1;
    }

    strcpy(dump->threadName,poll_th->threadName?poll_th->threadName:"");
    dump->pid = poll_th->pid;

    dump->num_fds = poll_th->num_fds;
    memcpy(dump->poll_fds,poll_th->poll_fds,sizeof(struct pollfd) * (MAX_FD_IN_POLL + 1));
    dump->num_entries = poll_th->num_entries;
    memcpy(dump->poll_entries,poll_th->poll_entries,sizeof(cc_poll_notify_entry_t) * MAX_FD_IN_POLL);

    return 0;
}






