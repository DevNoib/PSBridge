#ifndef PSBRIDGE_LOG_H
#define PSBRIDGE_LOG_H

#include <stdio.h>

#define PSB_LOG_MASTER 1
#define PSB_LOG_IMPORT 0
#define PSB_LOG_IO 0
#define PSB_LOG_THREAD 1
#define PSB_LOG_MBX 1
#define PSB_LOG_GE 0
#define PSB_LOG_GFX 0
#define PSB_LOG_AUDIO 0
#define PSB_LOG_MEMORY 0

#if PSB_LOG_MASTER
#define PSB_LOG(area, format, ...) do { if (PSB_LOG_##area) printf(format "\n", ##__VA_ARGS__); } while (0)
#else
#define PSB_LOG(area, format, ...) do { } while (0)
#endif

#endif
