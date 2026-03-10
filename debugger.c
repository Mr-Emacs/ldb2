#include <windows.h>
#include <minwinbase.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "types.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

DEBUG_ERROR debugger_load(Debugger *dbg, char *exe, char *args)
{
    NULL_CHECK(dbg);
    NULL_CHECK(exe);

    HANDLE hStdinRead,  hStdinWrite;
    HANDLE hStdoutRead, hStdoutWrite;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    if (!CreatePipe(&hStdinRead,  &hStdinWrite,  &sa, 0)) return DBG_LOAD_ERR;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) return DBG_LOAD_ERR;

    SetHandleInformation(hStdinWrite,  HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead,  HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError  = hStdoutWrite;

    char buffer[1024] = {0};
    if (args && args[0]) {
        snprintf(buffer, sizeof(buffer), "lldb.exe -- %s %s", exe, args);
    } else {
        snprintf(buffer, sizeof(buffer), "lldb.exe %s", exe);
    }

    if (!CreateProcessA(NULL, buffer, NULL, NULL,
                        TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi))
    {
        printf("CreateProcess failed (%lu)\n", GetLastError());
        CloseHandle(hStdinRead);  CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
        return DBG_LOAD_ERR;
    }

    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    dbg->hProcess    = pi.hProcess;
    dbg->hThread     = pi.hThread;
    dbg->hStdinWrite = hStdinWrite;
    dbg->hStdoutRead = hStdoutRead;
    dbg->pid         = pi.dwProcessId;
    dbg->thread_count = 0;
    dbg->current_thread_id = 0;
    dbg->bp_count = 0;

    return DBG_OK;
}

DEBUG_ERROR debugger_thread_info(Debugger *dbg)
{
    NULL_CHECK(dbg);
    send_to_dbg(dbg, "thread list\n");
    return DBG_OK;
}

DEBUG_ERROR debugger_thread_select(Debugger *dbg, u32 thread_id)
{
    NULL_CHECK(dbg);
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "thread select %u\n", thread_id);
    send_to_dbg(dbg, cmd);
    return DBG_OK;
}

DEBUG_ERROR debugger_array_view(Debugger *dbg, const char *array_expr, u32 count)
{
    NULL_CHECK(dbg);
    NULL_CHECK(array_expr);

    char cmd[512];

    if (strstr(array_expr, "*") || strstr(array_expr, "->")) {
        snprintf(cmd, sizeof(cmd), "parray %u %s\n", count, array_expr);
    } else {
        snprintf(cmd, sizeof(cmd), "expression -f hex -- %s[0..%u]\n", array_expr, count - 1);
    }

    send_to_dbg(dbg, cmd);
    return DBG_OK;
}

DEBUG_ERROR debugger_run(Debugger *dbg)
{
    NULL_CHECK(dbg);
    send_to_dbg(dbg, "run\n");
    return DBG_OK;
}

DEBUG_ERROR debugger_step_over(Debugger *dbg)
{
    NULL_CHECK(dbg);
    send_to_dbg(dbg, "next\n");
    return DBG_OK;
}

DEBUG_ERROR debugger_step_into(Debugger *dbg)
{
    NULL_CHECK(dbg);
    send_to_dbg(dbg, "step\n");
    return DBG_OK;
}

DEBUG_ERROR debugger_continue(Debugger *dbg)
{
    NULL_CHECK(dbg);
    send_to_dbg(dbg, "continue\n");
    return DBG_OK;
}

DEBUG_ERROR debugger_break(Debugger *dbg)
{
    NULL_CHECK(dbg);
    send_to_dbg(dbg, "process interrupt\n");
    return DBG_OK;
}

void send_to_dbg(Debugger *dbg, const char *cmd)
{
    NULL_CHECK(dbg);
    NULL_CHECK(cmd);
    DWORD wrote = 0;
    WriteFile(dbg->hStdinWrite, cmd, (DWORD)strlen(cmd), &wrote, NULL);
}

DWORD debugger_poll(Debugger *dbg, char *buf, u32 size)
{
    NULL_CHECK(dbg);
    DWORD available = 0;
    if (!PeekNamedPipe(dbg->hStdoutRead, NULL, 0, NULL, &available, NULL))
        return 0;
    if (available == 0)
        return 0;

    DWORD bytes_read = 0;
    ReadFile(dbg->hStdoutRead, buf, MIN(available, (DWORD)(size - 1)), &bytes_read, NULL);
    buf[bytes_read] = '\0';
    return bytes_read;
}

const char *get_err_string(DEBUG_ERROR err)
{
    static char message[512];

    switch (err) {
        case DBG_FILE_ERR:  sprintf(message, "Could not open expected file");       break;
        case DBG_SYM_ERR:   sprintf(message, "Could not load debug symbols");       break;
        case DBG_BRK_ERR:   sprintf(message, "Could not set a breakpoint");         break;
        case DBG_LOAD_ERR:  sprintf(message, "Could not load / spawn debugger");    break;
        case DBG_OK:        sprintf(message, "OK");                                  break;
        case DBG_COUNT:     sprintf(message, "(sentinel — not a real error)");       break;
        default:
            sprintf(message, "Unknown error");
            break;
    }
    return message;
}
