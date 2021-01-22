#pragma once

class HeavyTask;

#if CONFIG_HEAVY_TASK_ENABLED
void heavyTaskMain(void * parameters);
#endif

bool addLongTask(HeavyTask * ht);
void abortLongTask(bool pause = true);
void resumeLongTask();