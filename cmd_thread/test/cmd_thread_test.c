
#include "../cmd_thread.h"



void main()
{

    cc_poll_thread_t* poll_th = NULL;
    cc_poll_thread_launch(&poll_th,CC_POLL_TYPE_DATA,"yuanleihahaha");

    cc_poll_thread_add_fd(poll_th,1,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,2,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,3,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,4,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,5,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,6,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,7,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,8,NULL,NULL,TRUE);
    cc_poll_thread_add_fd(poll_th,9,NULL,NULL,TRUE);

    cc_poll_thread_dump_t dump;
    memset(&dump,0x00,sizeof(cc_poll_thread_dump_t));

    cc_poll_thread_dump(poll_th,&dump);

    CC_DEBUG("[TEST] poll thread pid = %lld , thread name:%s\n",(long long)dump.pid,dump.threadName);
    CC_DEBUG("[TEST] num fds:%d , %d %d %d %d %d %d %d %d %d %d %d\n",dump.num_fds,dump.poll_fds[0].fd,
                                    dump.poll_fds[1].fd,
                                    dump.poll_fds[2].fd,
                                    dump.poll_fds[3].fd,
                                    dump.poll_fds[4].fd,
                                    dump.poll_fds[5].fd,
                                    dump.poll_fds[6].fd,
                                    dump.poll_fds[7].fd,
                                    dump.poll_fds[8].fd,
                                    dump.poll_fds[9].fd,
                                    dump.poll_fds[10].fd);

    CC_DEBUG("[TEST] num entries:%d , %d %d %d %d %d %d %d %d %d %d\n",dump.num_entries,dump.poll_entries[0].fd,
                                    dump.poll_entries[1].fd,
                                    dump.poll_entries[2].fd,
                                    dump.poll_entries[3].fd,
                                    dump.poll_entries[4].fd,
                                    dump.poll_entries[5].fd,
                                    dump.poll_entries[6].fd,
                                    dump.poll_entries[7].fd,
                                    dump.poll_entries[8].fd,
                                    dump.poll_entries[9].fd);


    cc_poll_thread_del_fd(poll_th,3,TRUE);
    cc_poll_thread_del_fd(poll_th,4,TRUE);
    cc_poll_thread_del_fd(poll_th,5,TRUE);
    cc_poll_thread_del_fd(poll_th,12,TRUE);

    memset(&dump,0x00,sizeof(cc_poll_thread_dump_t));

    cc_poll_thread_dump(poll_th,&dump);

    CC_DEBUG("[TEST] num fds:%d , %d %d %d %d %d %d %d %d %d %d %d\n",dump.num_fds,dump.poll_fds[0].fd,
                                    dump.poll_fds[1].fd,
                                    dump.poll_fds[2].fd,
                                    dump.poll_fds[3].fd,
                                    dump.poll_fds[4].fd,
                                    dump.poll_fds[5].fd,
                                    dump.poll_fds[6].fd,
                                    dump.poll_fds[7].fd,
                                    dump.poll_fds[8].fd,
                                    dump.poll_fds[9].fd,
                                    dump.poll_fds[10].fd);

    CC_DEBUG("[TEST] num entries:%d , %d %d %d %d %d %d %d %d %d %d\n",dump.num_entries,dump.poll_entries[0].fd,
                                    dump.poll_entries[1].fd,
                                    dump.poll_entries[2].fd,
                                    dump.poll_entries[3].fd,
                                    dump.poll_entries[4].fd,
                                    dump.poll_entries[5].fd,
                                    dump.poll_entries[6].fd,
                                    dump.poll_entries[7].fd,
                                    dump.poll_entries[8].fd,
                                    dump.poll_entries[9].fd);


    cc_poll_thread_release(&poll_th);

    CC_DEBUG("[TEST] poll_th pointer:%p, exit main loop\n",poll_th);

    return;

}
