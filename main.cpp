/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.hpp"
#include "hal.h"
#include "test.h"

#include "chprintf.h"
#include "shell.h"
#include "shellapps.h"

#include "lwipthread.h"
#include "web/web.h"

#include "ff.h"
#include "fs.h"
#include "i2c.h"
#include <string.h>

using namespace chibios_rt;


/*===========================================================================*/
/* Configuration Parameters                                                  */
/*===========================================================================*/
static const SerialConfig serialcfg = {
    115200, // baud rate
    0,
    0,
    0,
};

/* I2C interface #2 */
static const I2CConfig i2c1cfg = {
    OPMODE_I2C,
    200000,
	FAST_DUTY_CYCLE_2,
};

/*===========================================================================*/
/* Thread Classes                                                            */
/*===========================================================================*/
#ifndef SEVENSEGMENT_THREAD_STACK_SIZE
#define SEVENSEGMENT_THREAD_STACK_SIZE   512
#endif

#define I2C_CNT_ADDR 0x20
static THD_WORKING_AREA(wa_sevensegment, SEVENSEGMENT_THREAD_STACK_SIZE);

static uint8_t rxbuf[2];
static uint8_t txbuf[2];
/*
 * Counter thread, times are in milliseconds.
 */
static THD_FUNCTION(sevensegment, p)
{
	(void)p;
	uint8_t count = 0;

	chRegSetThreadName("Blinker");

    // First set the direction registers
    txbuf[0] = 0x00; // direction reg
    txbuf[1] = 0x00; // all outputs
    i2cAcquireBus(&I2CD1);
    i2cMasterTransmitTimeout(&I2CD1, I2C_CNT_ADDR, txbuf, 2, rxbuf, 0, 1000);
    i2cReleaseBus(&I2CD1);


    while(true)
    {
        txbuf[0] = 0x14; // output reg
        txbuf[1] = count;
        i2cAcquireBus(&I2CD1);
        i2cMasterTransmitTimeout(&I2CD1, I2C_CNT_ADDR, txbuf, 2, rxbuf, 0, 1000);
        i2cReleaseBus(&I2CD1);
        count++;
        chThdSleepMilliseconds(100);
    }
}


/*===========================================================================*/
/* Main Entry Point                                                          */
/*===========================================================================*/
int main(void) {

    static thread_t *shelltp = NULL;
    static const evhandler_t evhndl[] = {
        InsertHandler,
        RemoveHandler
    };
    event_listener_t el0, el1;

  /**
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   **/
  halInit();
  System::init();

  /**
   * Initialize LwIP Subsystem.
   */
  lwipInit(NULL); // Use static ip, modify IP address in lwpthread.h

  /**
   * Activates the serial driver 2 using the driver default configuration.
   * PA2(TX) and PA3(RX) are routed to USART2.
   **/
  palSetPadMode(GPIOC, 6, PAL_MODE_ALTERNATE(8));
  palSetPadMode(GPIOC, 7, PAL_MODE_ALTERNATE(8));
  sdStart(&SD6, &serialcfg);

  /**
   * Initialize I2C1
   **/
  chprintf((BaseSequentialStream *) &SD6, "Initializing I2C...\r\n");
  i2cStart(&I2CD1, &i2c1cfg);

  /**
   * Shell manager initialization.
   **/
  shellInit();

  /*
   * Start the SDC/MMC Card Driver
   */
  chprintf((BaseSequentialStream *) &SD6, "Initializing Shell...\r\n");
  sdcStart(&SDCD1, NULL);

  /*
   * Activates the card insertion monitor.
   */
  tmr_init(&SDCD1);

  /*
   * Creates the HTTP thread (it changes priority internally).
   */
  chprintf((BaseSequentialStream *) &SD6, "Starting web server...\r\n");
  chThdCreateStatic(wa_http_server, sizeof(wa_http_server), NORMALPRIO + 1,
          http_server, NULL);

  /*
   * Creates the blinker thread.
   */
  chprintf((BaseSequentialStream *) &SD6, "Starting counter thread...\r\n");
  chThdCreateStatic(wa_sevensegment, sizeof(wa_sevensegment), NORMALPRIO + 1,
		  sevensegment, NULL);
  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and listen for events.
   */
  chEvtRegister(&inserted_event, &el0, 0);
  chEvtRegister(&removed_event, &el1, 1);

  /*
   * Serves timer events.
   */
  while (true) {
      if (!shelltp)
          shelltp = shellCreate(&shell_cfg, SHELL_WA_SIZE, NORMALPRIO);
      else if (chThdTerminatedX(shelltp)) {
          chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
          shelltp = NULL;           /* Triggers spawning of a new shell.        */
      }
      chEvtDispatch(evhndl, chEvtWaitOneTimeout(ALL_EVENTS, MS2ST(500)));
  }

  return 0;
}
