#include "ch.hpp"
#include "hal.h"
#include "shellapps.h"
#include "test.h"
#include "chprintf.h"
#include "shell.h"

#include "ff.h"

// #include "timeutils.h"
#include "shellutils.h"
#include "fs.h"

#include <string.h>
#include <stdlib.h>


/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/
static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
    size_t n, size;

    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: mem\r\n");
        return;
    }
    n = chHeapStatus(NULL, &size);
    chprintf(chp, "core free memory : %u bytes\r\n", chCoreGetStatusX());
    chprintf(chp, "heap fragments   : %u\r\n", n);
    chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
    static const char *states[] = {CH_STATE_NAMES};
    thread_t *tp;

    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: threads\r\n");
        return;
    }
#if (CH_CFG_USE_REGISTRY == TRUE)
    chprintf(chp, "    name	  addr    stack prio refs     state time\r\n");
#else
    chprintf(chp, "    addr     stack prio refs     state time\r\n");
#endif
    tp = chRegFirstThread();
    do {
        chprintf(chp,
#if (CH_CFG_USE_REGISTRY == TRUE)
                "%10s %08lx %08lx %4lu %4lu %9s\r\n",
#else
                "%08lx %08lx %4lu %4lu %9s\r\n",
#endif
#if (CH_CFG_USE_REGISTRY == TRUE)
                (const char *) tp->p_name,
#endif
                (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
                (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
                states[tp->p_state]);
        tp = chRegNextThread(tp);
    } while (tp != NULL);
}

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
    thread_t *tp;

    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: test\r\n");
        return;
    }
    tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriorityX(),
            TestThread, chp);
    if (tp == NULL) {
        chprintf(chp, "out of memory\r\n");
        return;
    }
    chThdWait(tp);
}

static const ShellCommand commands[] = {
    {"mem", cmd_mem},
    {"threads", cmd_threads},
    {"test", cmd_test},
    {"ls", cmd_tree},
	{"free", cmd_free },
    {"mkdir", cmd_mkdir},
    {"setlabel", cmd_setlabel},
    {"getlabel", cmd_getlabel},
    {"cat", cmd_cat},
    {NULL, NULL}
};

const ShellConfig shell_cfg = {
    (BaseSequentialStream *)&SD6,
    commands
};


