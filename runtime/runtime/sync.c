#include <delaythread.h>
#include <kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>

#include "psbridge.h"

#define EVENT_LIMIT 32
#define CALLBACK_LIMIT 32
#define WAIT_TIMEOUT ((int)0x800201a8u)
#define BAD_ID ((int)0x8002019au)

typedef struct {
    int used;
    int uid;
    u32 attributes;
    u32 pattern;
} EventFlag;

typedef struct {
    int used;
    int uid;
    int thread;
    u32 entry;
    u32 common;
    u32 pending;
    u32 argument;
} Callback;

static EventFlag event_flags[EVENT_LIMIT];
static Callback callbacks[CALLBACK_LIMIT];
static int next_event_uid = 0x2000;
static int next_callback_uid = 0x1800;
static int sync_lock = -1;
static u32 event_wait_count;
static u32 callback_notify_count;
static u32 callback_fire_count;

static u64 current_time(void)
{
    u32 seconds;
    u32 microseconds;

    TimerBusClock2USec(GetTimerSystemTime(), &seconds, &microseconds);
    return (u64)seconds * 1000000 + microseconds;
}

int psbridge_sync_init(void)
{
    ee_sema_t config;

    if (sync_lock >= 0)
        return 0;

    memset(&config, 0, sizeof(config));
    config.init_count = 1;
    config.max_count = 1;
    sync_lock = CreateSema(&config);
    return sync_lock < 0 ? sync_lock : 0;
}

static EventFlag *find_event(int uid)
{
    int index;

    for (index = 0; index < EVENT_LIMIT; index++) {
        if (event_flags[index].used && event_flags[index].uid == uid)
            return &event_flags[index];
    }

    return NULL;
}

static Callback *find_callback(int uid)
{
    int index;

    for (index = 0; index < CALLBACK_LIMIT; index++) {
        if (callbacks[index].used && callbacks[index].uid == uid)
            return &callbacks[index];
    }

    return NULL;
}

int psbridge_create_event(u32 attributes, u32 initial)
{
    int index;
    int uid;

    if (psbridge_sync_init() < 0)
        return -1;

    WaitSema(sync_lock);

    for (index = 0; index < EVENT_LIMIT; index++) {
        if (event_flags[index].used)
            continue;

        uid = next_event_uid++;
        event_flags[index].used = 1;
        event_flags[index].uid = uid;
        event_flags[index].attributes = attributes;
        event_flags[index].pattern = initial;
        SignalSema(sync_lock);
        printf(
            "[event] criou uid=%d atributos=%08x inicial=%08x thread=%d\n",
            uid,
            attributes,
            initial,
            GetThreadId()
        );
        return uid;
    }

    SignalSema(sync_lock);
    return -1;
}

int psbridge_delete_event(int uid)
{
    EventFlag *event;

    if (sync_lock < 0)
        return BAD_ID;

    WaitSema(sync_lock);
    event = find_event(uid);

    if (!event) {
        SignalSema(sync_lock);
        return BAD_ID;
    }

    memset(event, 0, sizeof(*event));
    SignalSema(sync_lock);
    return 0;
}

int psbridge_set_event(int uid, u32 bits)
{
    EventFlag *event;

    if (sync_lock < 0)
        return BAD_ID;

    WaitSema(sync_lock);
    event = find_event(uid);

    if (!event) {
        SignalSema(sync_lock);
        return BAD_ID;
    }

    event->pattern |= bits;
    SignalSema(sync_lock);
    return 0;
}

int psbridge_clear_event(int uid, u32 mask)
{
    EventFlag *event;

    if (sync_lock < 0)
        return BAD_ID;

    WaitSema(sync_lock);
    event = find_event(uid);

    if (!event) {
        SignalSema(sync_lock);
        return BAD_ID;
    }

    event->pattern &= mask;
    SignalSema(sync_lock);
    return 0;
}

int psbridge_wait_event(int uid, u32 bits, u32 mode, void *output, void *timeout, int use_callbacks, int polling)
{
    u32 *result = output ? psbridge_game_pointer(output) : NULL;
    u32 *remaining = timeout ? psbridge_game_pointer(timeout) : NULL;
    u32 limit = remaining ? *remaining : 0;
    u64 started = current_time();

    event_wait_count++;

    if (event_wait_count <= 16 || !(event_wait_count % 512))
        printf(
            "[event] espera n=%u uid=%d bits=%08x modo=%08x cb=%d poll=%d thread=%d\n",
            event_wait_count,
            uid,
            bits,
            mode,
            use_callbacks,
            polling,
            GetThreadId()
        );

    if ((!result && output) || (!remaining && timeout) || !bits || (mode & ~0x31u))
        return -1;

    if (sync_lock < 0)
        return BAD_ID;

    for (;;) {
        EventFlag *event;
        u32 pattern;
        u64 elapsed;
        int ready;

        WaitSema(sync_lock);
        event = find_event(uid);

        if (!event) {
            SignalSema(sync_lock);
            return BAD_ID;
        }

        pattern = event->pattern;
        ready = (mode & 1) ? (pattern & bits) != 0 : (pattern & bits) == bits;

        if (ready) {
            if (mode & 0x10)
                event->pattern = 0;
            else if (mode & 0x20)
                event->pattern &= ~bits;
        }

        SignalSema(sync_lock);

        if (ready) {
            if (result)
                *result = pattern;

            if (remaining) {
                elapsed = current_time() - started;
                *remaining = elapsed >= limit ? 0 : limit - (u32)elapsed;
            }

            return 0;
        }

        if (polling)
            return (int)0x800201afu;

        elapsed = current_time() - started;

        if (remaining && elapsed >= limit) {
            *remaining = 0;
            return WAIT_TIMEOUT;
        }

        if (use_callbacks)
            psbridge_check_callbacks();

        {
            u32 delay = remaining ? limit - (u32)elapsed : 1000;

            if (delay > 1000)
                delay = 1000;
            if (!delay)
                delay = 1;

            DelayThread(delay);
        }
    }
}

int psbridge_create_callback(u32 function, u32 common)
{
    int index;

    if (!function || psbridge_sync_init() < 0)
        return -1;

    WaitSema(sync_lock);

    for (index = 0; index < CALLBACK_LIMIT; index++) {
        Callback *callback = &callbacks[index];

        if (callback->used)
            continue;

        callback->used = 1;
        callback->uid = next_callback_uid++;
        callback->thread = GetThreadId();
        callback->entry = function;
        callback->common = common;
        callback->pending = 0;
        callback->argument = 0;
        SignalSema(sync_lock);
        printf(
            "[callback] criou uid=%d funcao=%08x comum=%08x thread=%d\n",
            callback->uid,
            function,
            common,
            callback->thread
        );
        return callback->uid;
    }

    SignalSema(sync_lock);
    return -1;
}

int psbridge_delete_callback(int uid)
{
    Callback *callback;

    if (sync_lock < 0)
        return BAD_ID;

    WaitSema(sync_lock);
    callback = find_callback(uid);

    if (!callback) {
        SignalSema(sync_lock);
        return BAD_ID;
    }

    memset(callback, 0, sizeof(*callback));
    SignalSema(sync_lock);
    return 0;
}

int psbridge_notify_callback(int uid, u32 argument)
{
    Callback *callback;

    if (sync_lock < 0)
        return BAD_ID;

    WaitSema(sync_lock);
    callback = find_callback(uid);

    if (!callback) {
        SignalSema(sync_lock);
        return BAD_ID;
    }

    if (callback->pending < 0x7fffffff)
        callback->pending++;

    callback->argument = argument;
    callback_notify_count++;
    SignalSema(sync_lock);

    if (callback_notify_count <= 16 || !(callback_notify_count % 256))
        printf(
            "[callback] notificou n=%u uid=%d argumento=%08x pendentes=%u thread=%d\n",
            callback_notify_count,
            uid,
            argument,
            callback->pending,
            GetThreadId()
        );

    return 0;
}

int psbridge_cancel_callback(int uid)
{
    Callback *callback;

    if (sync_lock < 0)
        return BAD_ID;

    WaitSema(sync_lock);
    callback = find_callback(uid);

    if (!callback) {
        SignalSema(sync_lock);
        return BAD_ID;
    }

    callback->pending = 0;
    SignalSema(sync_lock);
    return 0;
}

int psbridge_callback_count(int uid)
{
    Callback *callback;
    int count;

    if (sync_lock < 0)
        return BAD_ID;

    WaitSema(sync_lock);
    callback = find_callback(uid);
    count = callback ? (int)callback->pending : BAD_ID;
    SignalSema(sync_lock);
    return count;
}

int psbridge_check_callbacks(void)
{
    int current = GetThreadId();
    int fired = 0;
    int index;

    if (sync_lock < 0)
        return 0;

    for (index = 0; index < CALLBACK_LIMIT; index++) {
        Callback *callback = &callbacks[index];
        u32 function;
        u32 count;
        u32 argument;
        u32 common;
        int uid;
        int result;

        WaitSema(sync_lock);

        if (!callback->used || callback->thread != current || !callback->pending) {
            SignalSema(sync_lock);
            continue;
        }

        uid = callback->uid;
        function = callback->entry;
        count = callback->pending;
        argument = callback->argument;
        common = callback->common;
        callback->pending = 0;
        SignalSema(sync_lock);

        callback_fire_count++;

        if (callback_fire_count <= 16 || !(callback_fire_count % 256))
            printf(
                "[callback] executou n=%u uid=%d funcao=%08x vezes=%u argumento=%08x thread=%d\n",
                callback_fire_count,
                uid,
                function,
                count,
                argument,
                current
            );

        result = psbridge_call_callback(
            (void *)(uintptr_t)function,
            count,
            argument,
            common
        );

        fired++;

        if (result)
            psbridge_delete_callback(uid);
    }

    return fired;
}

int psbridge_delay_thread(u32 delay, int use_callbacks)
{
    u64 started;

    if (!use_callbacks)
        return DelayThread((int)delay);

    started = current_time();
    psbridge_check_callbacks();

    for (;;) {
        u64 elapsed = current_time() - started;
        u32 remaining;

        if (elapsed >= delay)
            return 0;

        remaining = delay - (u32)elapsed;

        if (remaining > 2000)
            remaining = 2000;

        DelayThread(remaining);
        psbridge_check_callbacks();
    }
}
