#include <debug.h>
#include <kernel.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "psbridge.h"

#define THREAD_ARG_SIZE 256
#define THREAD_LIMIT 8
#define THREAD_STACK_SIZE 0x10000
#define KERNEL_STACK_SIZE 0x1000
#define MAILBOX_LIMIT 8
#define MAILBOX_MESSAGES 32

typedef struct {
    int used;
    int uid;
    int thread_id;
    void *entry;
    u32 arg_size;
    void *arg;
    unsigned char arg_copy[THREAD_ARG_SIZE];
    void *kernel_work;
} PspThread;

typedef struct {
    int lock_level;
    int lock_thread;
    int attributes;
    int waiting_threads;
    int uid;
    int padding[3];
} PspLwMutex;

typedef struct {
    int used;
    int uid;
    int semaphore;
    int lock;
    u32 messages[MAILBOX_MESSAGES];
    int read_index;
    int write_index;
    int count;
} PspMailbox;

extern void *_gp;

extern int psbridge_call_thread(
    void *entry,
    u32 arg_size,
    void *arg,
    void *kernel_work
);

static PspThread threads[THREAD_LIMIT];
static int next_lwmutex_uid = 0x1000;

static PspMailbox mailboxes[MAILBOX_LIMIT];
static int next_mailbox_uid = 0x3000;

static unsigned char thread_stacks[THREAD_LIMIT] [THREAD_STACK_SIZE]
    __attribute__((aligned(16)));

static PspThread *find_thread(int uid)
{
    int index;

    for (index = 0; index < THREAD_LIMIT; index++) {
        if (threads[index].used && threads[index].uid == uid)
            return &threads[index];
    }
    return NULL;
}

static PspMailbox *find_mailbox(int uid)
{
    int index;

    for (index = 0; index < MAILBOX_LIMIT; index++) {
        if (mailboxes[index].used && mailboxes[index].uid == uid)
            return &mailboxes[index];
    }

    return NULL;
}

static void run_thread(void *arg)
{
    PspThread *thread = arg;
    u32 *kernel_work = thread->kernel_work;
    u32 *initial_stack = (u32 *) (
        (u8 *)thread->kernel_work -
        THREAD_STACK_SIZE +
        KERNEL_STACK_SIZE
    );

    int result;

    memset(kernel_work, 0, KERNEL_STACK_SIZE);

    kernel_work[0xc0 / 4] = thread->uid;
    kernel_work[0xc8 / 4] = (u32) (uintptr_t) initial_stack;
    kernel_work[0xf8 / 4] = 0xffffffff;
    kernel_work[0xfc / 4] = 0xffffffff;
    initial_stack[0] = thread->uid;

    scr_printf(
        "entrando na thread %d em 0x%08x\n",
        thread->uid,
        (u32)(uintptr_t)thread->entry
    );

    result = psbridge_call_thread(
        thread->entry,
        thread->arg_size,
        thread->arg,
        thread->kernel_work
    );

    scr_printf("thread %d gozou ops quero dizer voltou com %d\n", thread->uid, result);

    thread->used = 0;
    ExitDeleteThread();

}

int psbridge_create_thread(const char *name, void *entry, int priority, int stack_size, u32 attributes, void *options)
{
    int index;
    (void)stack_size;
    (void)attributes;
    (void)options;

    for (index = 0; index < THREAD_LIMIT; index++) {
        ee_thread_t config;
        int thread_id;

        if (threads[index].used)
            continue;

        if (priority < 2)
            priority = 2;

        if (priority > 126)
            priority = 126;

        memset(&config, 0, sizeof(config));

        config.func = run_thread;
        config.stack = thread_stacks[index];
        config.stack_size = THREAD_STACK_SIZE - KERNEL_STACK_SIZE;
        config.gp_reg = &_gp;
        config.initial_priority = priority;

        thread_id = CreateThread(&config);

        if (thread_id < 0)
            return thread_id;

        threads[index].used = 1;
        threads[index].uid = 0x100 + index;
        threads[index].thread_id = thread_id;
        threads[index].entry = entry;
        threads[index].kernel_work =
            thread_stacks[index] +
            THREAD_STACK_SIZE -
            KERNEL_STACK_SIZE;

        scr_printf("thread criada na puta que pariu %d\n", threads[index].uid);

        return threads[index].uid;
    }

    return -1;
}

int psbridge_start_thread(int uid, u32 arg_size, void *arg)
{
    PspThread *thread = find_thread(uid);
    int result;

    if (!thread)
        return -1;

    thread->arg_size = arg_size;
    thread->arg = NULL;

    if (arg_size && arg) {
        u32 copy_size = arg_size;

        if (copy_size > THREAD_ARG_SIZE)
        copy_size = THREAD_ARG_SIZE;

        memcpy(
            thread->arg_copy,
            psbridge_game_pointer(arg),
            copy_size
        );

        thread->arg_size = copy_size;
        thread->arg = thread->arg_copy;
    }

    result = StartThread(thread->thread_id, thread);

    if (result < 0)
        return result;

    scr_printf("thread iniciada uid %d\n", uid);

    return 0;
}

int psbridge_create_lwmutex(void *workarea, u32 attributes, int initial_count)
{
    PspLwMutex *mutex = psbridge_game_pointer(workarea);

    if (!mutex)
        return -1;

    memset(mutex, 0, sizeof(*mutex));
    mutex->lock_level = initial_count;
    mutex->attributes = attributes;
    mutex->uid = next_lwmutex_uid++;
    return 0;
}

int psbridge_delete_lwmutex(void *workarea)
{
    PspLwMutex *mutex = psbridge_game_pointer(workarea);

    if (!mutex)
        return -1;

    memset(mutex, 0, sizeof(*mutex));
    return 0;
}

int psbridge_lock_lwmutex(void *workarea, int count)
{
    PspLwMutex *mutex = psbridge_game_pointer(workarea);

    if (!mutex || count <= 0)
        return -1;

    mutex->lock_level += count;
    mutex->lock_thread = 1;
    return 0;
}

int psbridge_unlock_lwmutex(void *workarea, int count)
{
    PspLwMutex *mutex = psbridge_game_pointer(workarea);

    if (!mutex || count <= 0 || mutex->lock_level < count)
        return -1;

    mutex->lock_level -= count;

    if (!mutex->lock_level)
        mutex->lock_thread = 0;

    return 0;
}

int psbridge_create_mailbox(const char *name, u32 attributes, void *options)
{
    int index;
    (void)name;
    (void)attributes;
    (void)options;

    for (index = 0; index < MAILBOX_LIMIT; index++) {
        PspMailbox *mailbox;
        ee_sema_t config;
        int semaphore;
        int lock;

        if (mailboxes[index].used)
            continue;

        memset(&config, 0, sizeof(config));

        config.init_count = 0;
        config.max_count = MAILBOX_MESSAGES;

        semaphore = CreateSema(&config);

        if (semaphore < 0)
             return semaphore;
           

        memset(&config, 0, sizeof(config));
        config.init_count = 1;
        config.max_count = 1;

        lock = CreateSema(&config);

        if (lock < 0) {
            DeleteSema(semaphore);
            return lock;
        }

        mailbox = &mailboxes[index];
        memset(mailbox, 0, sizeof(*mailbox));

        mailbox->used = 1;
        mailbox->uid = next_mailbox_uid++;
        mailbox->semaphore = semaphore;
        mailbox->lock = lock;

        printf("mbx criou %d\n", mailbox->uid);

        

        return mailbox->uid;


    }

    return -1;
}

int psbridge_send_mailbox(int uid, void *message)
{
    PspMailbox *mailbox = find_mailbox(uid);
    int result;

    if (!mailbox || !message)
        return -1;

    if (!psbridge_game_pointer(message))
        return -1;

    result = WaitSema(mailbox->lock);

    if (result < 0)
        return result;

    if (mailbox->count >= MAILBOX_MESSAGES) {
        SignalSema(mailbox->lock);
        return -1;
    }

    mailbox->messages[mailbox->write_index] =
        (u32) (uintptr_t)message;

    mailbox->write_index =
        (mailbox->write_index +1) % MAILBOX_MESSAGES;

    mailbox->count++;

    SignalSema(mailbox->lock);
    SignalSema(mailbox->semaphore);

    return 0;
}

int psbridge_receive_mailbox(int uid, void *output, void *timeout)
{
    PspMailbox *mailbox = find_mailbox(uid);
    u32 *destination = psbridge_game_pointer(output);
    int result;

    (void)timeout;

    if (!mailbox || !destination)
        return -1;

    result = WaitSema(mailbox->semaphore);

    if (result < 0)
        return result;

    result = WaitSema(mailbox->lock);

    if (result < 0)
        return result;

    if (mailbox->count <= 0) {
        SignalSema(mailbox->lock);
        return -1;
    }

    *destination = mailbox->messages[mailbox->read_index];

    mailbox->read_index =
        (mailbox->read_index + 1) % MAILBOX_MESSAGES;

    mailbox->count--;

    printf(
        "mbl recebou uid %d msg %08x fila %d thread %d\n",
        uid,
        *destination,
        mailbox->count,
        GetThreadId()
    );

    SignalSema(mailbox->lock);

    return 0;
}

int psbridge_delete_mailbox(int uid)
{
    PspMailbox *mailbox = find_mailbox(uid);

    if (!mailbox)
        return -1;

    DeleteSema(mailbox->semaphore);
    DeleteSema(mailbox->lock);

    memset(mailbox, 0, sizeof(*mailbox));

    return 0;
}