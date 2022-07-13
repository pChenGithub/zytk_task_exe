#include "task_exe.h"
#include "lombo_system_sound.h"
#include "global.h"
#include "signalsender_interface.h"
#include "file_ops.h"
#include "wifi_station.h"
#include "base/tts/interface_ttsOps.h"
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <stdlib.h>

typedef struct
{
    pthread_t task_exe_pid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    ee_queue_t task_exe_req;
    char pthread_stop;
} task_exe_svc;

static task_exe_svc _task_exe = {0};

int task_exe_set_volume(void* arg)
{
    int* volume = (int*)arg;
    lombo_system_set_volume(*volume);
    g_BaseInfo.playVolume = *volume;
    lombo_system_sound_play(WELCOME_SOUND);

    return 0;
}
// 播放字符串任务
int task_exe_PlayString(void *str)
{
    if(str == NULL)
    {
        LOG_E("cmd err!");
        return -1;
    }
    LOG_I("播放字符串 %s", (char*)str);
    tts_play_string(str);
    return 0;
}

// 播放音频任务
int task_exe_PlayWav(void *file)
{
    if(file == NULL)
    {
        LOG_E("cmd err!");
        return -1;
    }
    lombo_system_sound_play((char*)file);
    return 0;
}

// 执行shell命令任务
int task_exe_cmd(void *cmd)
{
    if(cmd == NULL)
    {
        LOG_E("cmd err!");
        return -1;
    }
    LOG_D("cmd: %s ", (char*)cmd);
    system((char*)cmd);
    free(cmd);
    cmd = NULL;
    return 0;
}

// 拷贝wifi ssid
int task_exe_WifiSsidCopy(void* arg)
{
    (void)arg;
#if 1
    CONNECT_INFO connect_info;
    memset(&connect_info, 0, sizeof(connect_info));
    if (0==sta_get_connect_info(&connect_info))
    {
        memcpy(g_WifiInfo.wifi_Ssid, connect_info.ssid, WIFI_SSID_LEN);
        (g_WifiInfo.wifi_Ssid)[WIFI_SSID_LEN-1] = 0;
    }
#endif
    return 0;
}

int task_exe_init()
{
    pthread_mutex_init(&_task_exe.mutex, NULL);
    pthread_cond_init(&_task_exe.cond, NULL);

    ee_queue_t* h = &_task_exe.task_exe_req;
    ee_queue_init(h);
    return 0;
}

void task_exe_deinit()
{
    pthread_mutex_destroy(&_task_exe.mutex);
    pthread_cond_destroy(&_task_exe.cond);
}

//检查队列，执行任务
static void* task_exe_exe(void* arg)
{
    (void)arg;
    ee_queue_t* h = &_task_exe.task_exe_req;
    task_exe_req* req = NULL;
    ee_queue_t* pos = NULL, *n = NULL;
    task_exe_func func = NULL;
    prctl(PR_SET_NAME, "task_exe");
    while (!_task_exe.pthread_stop)
    {
        pthread_mutex_lock(&_task_exe.mutex);
        // 阻塞,等待唤醒
        pthread_cond_wait(&_task_exe.cond, &_task_exe.mutex);
        pthread_mutex_unlock(&_task_exe.mutex);

       // pthread_mutex_lock(&_task_exe.mutex);
        // 任务队列为空，返回
        if (ee_queue_empty(h))
        {
            //pthread_mutex_unlock(&_task_exe.mutex);
            continue;
        }

        // 任务出队，执行，然后free
        ee_queue_for_each_safe(pos, n, h)
        {
            req = ee_queue_data(pos, task_exe_req, queue);
            if (NULL!=req)
            {
                // 执行任务
                for (int i=0;i<req->task_num;i++)
                {
                    func = req->tasks[i];
                    func(req->args[i]);
                }

                ee_queue_remove(&req->queue);
                free(req);
            }
        }
        //pthread_mutex_unlock(&_task_exe.mutex);
    }
    return NULL;
}

//启动任务执行服务，该服务接受提交任务，然后执行
int task_exe_start()
{
    _task_exe.pthread_stop = 0;
    pthread_create(&_task_exe.task_exe_pid, NULL, task_exe_exe, NULL);
    pthread_detach(_task_exe.task_exe_pid);
    return 0;
}

void task_exe_stop()
{
    _task_exe.pthread_stop = 1;
    pthread_join(_task_exe.task_exe_pid, NULL);
}

int task_exe_CommitReq(task_exe_req *req)
{
    if (NULL==req)
    {
        LOG_E("提交任务执行请求，参数为空，错误返回");
        return -1;
    }
    if (0==_task_exe.task_exe_pid || _task_exe.pthread_stop)
    {
        LOG_E("执行任务线程未开启，错误退出");
        free(req);
        return -1;
    }
    ee_queue_t* h = &_task_exe.task_exe_req;

    LOG_I("唤醒线程执行任务");
    if (0!=pthread_mutex_trylock(&_task_exe.mutex))
    {
        LOG_E("获取锁失败，唤醒失败\n");
        free(req);
        return -1;
    }

    // 提交任务
    ee_queue_insert_tail(h, &req->queue);
    // 唤醒线程
    pthread_cond_signal(&_task_exe.cond);
    pthread_mutex_unlock(&_task_exe.mutex);
    LOG_I("成功提交一个任务，准备执行");
    return 0;
}
