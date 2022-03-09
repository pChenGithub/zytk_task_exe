/**
 * 执行提交的任务
*/

#ifndef _TASK_EXE_H_
#define _TASK_EXE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ee_queue.h"

#define TASK_LEN 5
typedef int (*task_exe_func)(void*);

typedef struct {
    // 自定义内容
    int task_num;
    task_exe_func tasks[TASK_LEN];
    void* args[TASK_LEN];
    ee_queue_t queue;
    char str[16];                   // 一个比较短的字符串
} task_exe_req;

// 播放字符串任务
int task_exe_PlayString(void *str);
// 播放音频任务
int task_exe_PlayWav(void *file);
int task_exe_cmd(void *cmd);
//
int task_exe_WifiSsidCopy(void* arg);

int task_exe_init();
void task_exe_deinit();
int task_exe_start();
void task_exe_stop();

int task_exe_CommitReq(task_exe_req* req);

#ifdef __cplusplus
}
#endif
#endif
