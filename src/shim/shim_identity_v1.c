/* PATCH-004 — BUILD_ID + optional workload_id on stderr (B-CONC-12). */
#include "shim_internal.h"

#include <stdio.h>
#include <stdlib.h>

static int g_identity_emitted;

const char *hwatom_shim_build_id(void)
{
    const char *e = getenv("HWATOM_BUILD_ID");

    if (e && e[0]) {
        return e;
    }
#ifdef HWATOM_BUILD_ID
    return HWATOM_BUILD_ID;
#else
    return "dev";
#endif
}

void hwatom_shim_emit_identity_once(void)
{
    const char *wl;

    if (g_identity_emitted) {
        return;
    }
    g_identity_emitted = 1;
    fprintf(stderr, "HWATOM_BUILD_ID=%s\n", hwatom_shim_build_id());
#ifdef HWATOM_SHIM_VERSION
    fprintf(stderr, "HWATOM_SHIM_VERSION=%s\n", HWATOM_SHIM_VERSION);
#endif
    wl = getenv("HWATOM_WORKLOAD_ID");
    if (wl && wl[0]) {
        fprintf(stderr, "workload_id=%s\n", wl);
    }
}
