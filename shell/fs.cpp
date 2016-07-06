/*
 * fat.c
 *
 *  Created on: Dec 19, 2013
 *      Author: Jed Frey
 */

/*===========================================================================*/
/* FatFs related.                                                            */
/*===========================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"

#include "chprintf.h"
#include "shell.h"

#include "usb_cdc.h"
#include "fs.h"

#include "ff.h"

/**
 * @brief FS object.
 */
FATFS SDC_FS;

/* FS mounted and ready.*/
bool fs_ready = FALSE;

/* Generic large buffer.*/
static uint8_t fbuff[1024];

/*===========================================================================*/
/* Card insertion monitor.                                                   */
/*===========================================================================*/

#define POLLING_INTERVAL                10
#define POLLING_DELAY                   10

/**
 * @brief   Card monitor timer.
 */
static virtual_timer_t tmr;

/**
 * @brief   Debounce counter.
 */
static unsigned cnt;

/**
 * @brief   Card event sources.
 */
event_source_t inserted_event, removed_event;

/*===========================================================================*/
/* Event Handlers                                                            */
/*===========================================================================*/

/*
 * Card insertion event.
 */
void InsertHandler(eventid_t id) 
{
  FRESULT err;

  (void)id;
  /*
   * On insertion SDC initialization and FS mount.
   */
  if (sdcConnect(&SDCD1))
    return;

  err = f_mount(&SDC_FS, "/", 1);
  if (err != FR_OK) {
    sdcDisconnect(&SDCD1);
    return;
  }
  fs_ready = TRUE;
}

/*
 * Card removal event.
 */
void RemoveHandler(eventid_t id) 
{

  (void)id;
  sdcDisconnect(&SDCD1);
  fs_ready = FALSE;
}

/*===========================================================================*/
/* Card Monitors                                                             */
/*===========================================================================*/

/**
 * @brief   Insertion monitor timer callback function.
 *
 * @param[in] p         pointer to the @p BaseBlockDevice object
 *
 * @notapi
 */
static void tmrfunc(void *p) {
  BaseBlockDevice *bbdp = (BaseBlockDevice *) p;

  chSysLockFromISR();
  if (cnt > 0) {
    if (blkIsInserted(bbdp)) {
      if (--cnt == 0) {
        chEvtBroadcastI(&inserted_event);
      }
    }
    else
      cnt = POLLING_INTERVAL;
  }
  else {
    if (!blkIsInserted(bbdp)) {
      cnt = POLLING_INTERVAL;
      chEvtBroadcastI(&removed_event);
    }
  }
  chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, bbdp);
  chSysUnlockFromISR();
}

/**
 * @brief   Polling monitor start.
 *
 * @param[in] p         pointer to an object implementing @p BaseBlockDevice
 *
 * @notapi
 */
void tmr_init(void *p) 
{

  chEvtObjectInit(&inserted_event);
  chEvtObjectInit(&removed_event);
  chSysLock();
  cnt = POLLING_INTERVAL;
  chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, p);
  chSysUnlock();
}


/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

/*
 * Scan Files in a path and print them to the character stream.
 */
FRESULT scan_files(BaseSequentialStream *chp, char *path) {
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int fyear,fmonth,fday,fhour,fminute,fsecond;

    int i;
    char *fn;

#if _USE_LFN
    fno.lfname = 0;
    fno.lfsize = 0;
#endif
    /*
     * Open the Directory.
     */
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        /*
         * If the path opened successfully.
         */
        i = strlen(path);
        while (true) {
            /*
             * Read the Directory.
             */
            res = f_readdir(&dir, &fno);
            /*
             * If the directory read failed or the
             */
            if (res != FR_OK || fno.fname[0] == 0) {
                break;
            }
            /*
             * If the directory or file begins with a '.' (hidden), continue
             */
            if (fno.fname[0] == '.') {
                continue;
            }
            fn = fno.fname;
            /*
             * Extract the date.
             */
            fyear = ((0b1111111000000000&fno.fdate) >> 9)+1980;
            fmonth= (0b0000000111100000&fno.fdate) >> 5;
            fday  = (0b0000000000011111&fno.fdate);
            /*
             * Extract the time.
             */
            fhour   = (0b1111100000000000&fno.ftime) >> 11;
            fminute = (0b0000011111100000&fno.ftime) >> 5;
            fsecond = (0b0000000000011111&fno.ftime)*2;
            /*
             * Print date and time of the file.
             */
            chprintf(chp, "%4d-%02d-%02d %02d:%02d:%02d ", fyear, fmonth, fday, fhour, fminute, fsecond);
            /*
             * If the 'file' is a directory.
             */
            if (fno.fattrib & AM_DIR) {
                /*
                 * Add a slash to the end of the path
                 */
                path[i++] = '/';
                strcpy(&path[i], fn);
                /*
                 * Print that it is a directory and the path.
                 */
                chprintf(chp, "<DIR> %s/\r\n", path);
                /*
                 * Recursive call to scan the files.
                 */
                res = scan_files(chp, path);
                if (res != FR_OK) {
                    break;
                }
                path[--i] = 0;
            } else {
                /*
                 * Otherwise print the path as a file.
                 */
                chprintf(chp, "      %s/%s\r\n", path, fn);
            }
        }
    } else {
        chprintf(chp, "FS: f_opendir() failed\r\n");
    }
    return res;
}


void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]) {
    FRESULT err;
    uint32_t clusters;
    FATFS *fsp;

    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: tree\r\n");
        return;
    }
    if (!fs_ready) {
        chprintf(chp, "File System not mounted\r\n");
        return;
    }
    err = f_getfree("/", &clusters, &fsp);
    if (err != FR_OK) {
        chprintf(chp, "FS: f_getfree() failed\r\n");
        return;
    }
    chprintf(chp,
            "FS: %lu free clusters, %lu sectors per cluster, %lu bytes free\r\n",
            clusters, (uint32_t)SDC_FS.csize,
            clusters * (uint32_t)SDC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE);
    fbuff[0] = 0;
    scan_files(chp, (char *)fbuff);
}


void cmd_free(BaseSequentialStream *chp, int argc, char *argv[]) {
    FRESULT err;
    uint32_t clusters;
    FATFS *fsp;
    (void)argc;
    (void)argv;

    err = f_getfree("/", &clusters, &fsp);
    if (err != FR_OK) {
        chprintf(chp, "FS: f_getfree() failed\r\n");
        return;
    }
    /*
     * Print the number of free clusters and size free in B, KiB and MiB.
     */
    chprintf(chp,"FS: %lu free clusters\r\n    %lu sectors per cluster\r\n",
            clusters, (uint32_t)SDC_FS.csize);
    chprintf(chp,"%lu B free\r\n",
            clusters * (uint32_t)SDC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE);
    chprintf(chp,"%lu KB free\r\n",
            (clusters * (uint32_t)SDC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE)/(1024));
    chprintf(chp,"%lu MB free\r\n",
            (clusters * (uint32_t)SDC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE)/(1024*1024));
}

void cmd_mkdir(BaseSequentialStream *chp, int argc, char *argv[]) {
    FRESULT err;
    if (argc != 1) {
        chprintf(chp, "Usage: mkdir dirName\r\n");
        chprintf(chp, "       Creates directory with dirName (no spaces)\r\n");
        return;
    }
    /*
     * Attempt to make the directory with the name given in argv[0]
     */
    err=f_mkdir(argv[0]);
    if (err != FR_OK) {
        /*
         * Display failure message and reason.
         */
        chprintf(chp, "FS: f_mkdir(%s) failed\r\n",argv[0]);
        verbose_error(chp, err);
        return;
    } else {
        chprintf(chp, "FS: f_mkdir(%s) succeeded\r\n",argv[0]);
    }
    return;
}

void cmd_setlabel(BaseSequentialStream *chp, int argc, char *argv[]) {
    FRESULT err;
    if (argc != 1) {
        chprintf(chp, "Usage: setlabel label\r\n");
        chprintf(chp, "       Sets FAT label (no spaces)\r\n");
        return;
    }
    /*
     * Attempt to set the label with the name given in argv[0].
     */
    err=f_setlabel(argv[0]);
    if (err != FR_OK) {
        chprintf(chp, "FS: f_setlabel(%s) failed.\r\n");
        verbose_error(chp, err);
        return;
    } else {
        chprintf(chp, "FS: f_setlabel(%s) succeeded.\r\n");
    }
    return;
}

void cmd_getlabel(BaseSequentialStream *chp, int argc, char *argv[]) {
    FRESULT err;
    char lbl[12];
    DWORD sn;
    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: getlabel\r\n");
        chprintf(chp, "       Gets and prints FAT label\r\n");
        return;
    }
    memset(lbl,0,sizeof(lbl));
    /*
     * Get volume label & serial of the default drive
     */
    err = f_getlabel("", lbl, &sn);
    if (err != FR_OK) {
        chprintf(chp, "FS: f_getlabel failed.\r\n");
        verbose_error(chp, err);
        return;
    }
    /*
     * Print the label and serial number
     */
    chprintf(chp, "LABEL: %s\r\n",lbl);
    chprintf(chp, "  S/N: 0x%X\r\n",sn);
    return;
}

/*
 * Print a text file to screen
 */
void cmd_cat(BaseSequentialStream *chp, int argc, char *argv[]) {
    FRESULT err;
    FIL fsrc;   /* file object */
    char Buffer[255];
    UINT ByteToRead=sizeof(Buffer);
    UINT ByteRead;
    /*
     * Print usage
     */
    if (argc != 1) {
        chprintf(chp, "Usage: cat filename\r\n");
        chprintf(chp, "       Echos filename (no spaces)\r\n");
        return;
    }
    /*
     * Attempt to open the file, error out if it fails.
     */
    err=f_open(&fsrc, argv[0], FA_READ);
    if (err != FR_OK) {
        chprintf(chp, "FS: f_open(%s) failed.\r\n",argv[0]);
        verbose_error(chp, err);
        return;
    }
    /*
     * Do while the number of bytes read is equal to the number of bytes to read
     * (the buffer is filled)
     */
    do {
        /*
         * Clear the buffer.
         */
        memset(Buffer,0,sizeof(Buffer));
        /*
         * Read the file.
         */
        err=f_read(&fsrc,Buffer,ByteToRead,&ByteRead);
        if (err != FR_OK) {
            chprintf(chp, "FS: f_read() failed\r\n");
            verbose_error(chp, err);
            f_close(&fsrc);
            return;
        }
        chprintf(chp, "%s", Buffer);
    } while (ByteRead>=ByteToRead);
    chprintf(chp,"\r\n");
    /*
     * Close the file.
     */
    f_close(&fsrc);
    return;
}

void verbose_error(BaseSequentialStream *chp, FRESULT err) {
    chprintf(chp, "\t%s.\r\n",fresult_str(err));
}

const char* fresult_str(FRESULT stat) 
{
    char str[255];
    memset(str,0,sizeof(str));
    switch (stat) {
        case FR_OK:
            return "Succeeded";
        case FR_DISK_ERR:
            return "A hard error occurred in the low level disk I/O layer";
        case FR_INT_ERR:
            return "Assertion failed";
        case FR_NOT_READY:
            return "The physical drive cannot work";
        case FR_NO_FILE:
            return "Could not find the file";
        case FR_NO_PATH:
            return "Could not find the path";
        case FR_INVALID_NAME:
            return "The path name format is invalid";
        case FR_DENIED:
            return "Access denied due to prohibited access or directory full";
        case FR_EXIST:
            return "Access denied due to prohibited access";
        case FR_INVALID_OBJECT:
            return "The file/directory object is invalid";
        case FR_WRITE_PROTECTED:
            return "The physical drive is write protected";
        case FR_INVALID_DRIVE:
            return "The logical drive number is invalid";
        case FR_NOT_ENABLED:
            return "The volume has no work area";
        case FR_NO_FILESYSTEM:
            return "There is no valid FAT volume";
        case FR_MKFS_ABORTED:
            return "The f_mkfs() aborted due to any parameter error";
        case FR_TIMEOUT:
            return "Could not get a grant to access the volume within defined period";
        case FR_LOCKED:
            return "The operation is rejected according to the file sharing policy";
        case FR_NOT_ENOUGH_CORE:
            return "LFN working buffer could not be allocated";
        case FR_TOO_MANY_OPEN_FILES:
            return "Number of open files > _FS_SHARE";
        case FR_INVALID_PARAMETER:
            return "Given parameter is invalid";
        default:
            return "Unknown";
    }
    return "";
}

