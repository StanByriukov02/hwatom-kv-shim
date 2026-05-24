#include "shim_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pthread_mutex_t g_map_epoch_mu = PTHREAD_MUTEX_INITIALIZER;
int g_map_epoch_active = 0;

void shim_trace(const char *epoch_tag, const char *op, const char *detail)
{
    if (!shim_log_enabled()) {
        return;
    }
    fprintf(stderr, "SHIM_TRACE %s %s %s\n", epoch_tag, op, detail ? detail : "");
}

int shim_epoch_try_enter(const char *op)
{
    (void)op;
    return 1;
}

void shim_epoch_abort(const char *op)
{
    shim_reserve_abort_pending();
    shim_trace("abort", op, "driver_fail");
}

void shim_epoch_exit(const char *op)
{
    (void)op;
    shim_slot_clear_all();
    pthread_mutex_lock(&g_map_epoch_mu);
    g_map_epoch_active = 0;
    pthread_mutex_unlock(&g_map_epoch_mu);
    shim_trace("exit", op, "ok");
}

int shim_epoch_held(void)
{
    return shim_reservation_active();
}
