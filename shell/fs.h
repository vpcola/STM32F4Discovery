/*
 * fs.h
 *
 *  Created on: Dec 19, 2015
 *      Author: Vergil Cola
 */

#include "ff.h"

#ifndef _FS_H_
#define _FS_H_

#include "ch.hpp"
#include "hal.h"
#include "ff.h"


#ifdef __cplusplus
extern "C" {
#endif

extern FATFS SDC_FS;
extern bool fs_ready;
extern event_source_t inserted_event, removed_event;
void InsertHandler(eventid_t id);
void RemoveHandler(eventid_t id);
void tmr_init(void *p); 


FRESULT scan_files(BaseSequentialStream *chp, char *path);
void cmd_free(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_setlabel(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_getlabel(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_hello(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_mkdir(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_cat(BaseSequentialStream *chp, int argc, char *argv[]);
void verbose_error(BaseSequentialStream *chp, FRESULT err);
const char* fresult_str(FRESULT stat);

#ifdef __cplusplus
}
#endif

#endif /* _FS_H_ */
