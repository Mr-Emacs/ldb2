#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <windows.h>
#include <stdio.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t   s32;
typedef float    f32;
typedef double   f64;
typedef i32      b32;
typedef i64      b64;

typedef enum {
    DBG_FILE_ERR = 1,
    DBG_SYM_ERR,
    DBG_BRK_ERR,
    DBG_LOAD_ERR,
    DBG_OK,
    DBG_COUNT,
} DEBUG_ERROR;

typedef struct {
    u32 thread_id;
    char name[64];
    char status[32];
    char location[256];
    b32 is_stopped;
    b32 is_current;
} ThreadInfo;

typedef struct {
    u32 bp_id;
    u32 line;
    char file[256];
    char condition[256];
    u32 hit_count;
    b32 is_enabled;
} BreakpointInfo;

typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdinWrite;
    HANDLE hStdoutRead;
    u32 pid;
    ThreadInfo threads[32];
    u32 thread_count;
    u32 current_thread_id;
    BreakpointInfo breakpoints[128];
    u32 bp_count;
} Debugger;


DEBUG_ERROR debugger_load(Debugger *dbg, char *exe, char *args);
DEBUG_ERROR debugger_run(Debugger *dbg);
DEBUG_ERROR debugger_step_over(Debugger *dbg);
DEBUG_ERROR debugger_step_into(Debugger *dbg);
DEBUG_ERROR debugger_continue(Debugger *dbg);
DEBUG_ERROR debugger_break(Debugger *dbg);
DEBUG_ERROR debugger_thread_info(Debugger *dbg);
DEBUG_ERROR debugger_thread_select(Debugger *dbg, u32 thread_id);
DEBUG_ERROR debugger_registers(Debugger *dbg);
void        send_to_dbg(Debugger *dbg, const char *cmd);
DWORD       debugger_poll(Debugger *dbg, char *buf, u32 size);


#define NULL_CHECK(x)                                          \
do {                                                           \
    if ((x) == NULL) {                                         \
        printf("%s:%d: Got null %p", __FILE__, __LINE__, (x)); \
    }                                                          \
} while(0)

#endif //TYPES_H
