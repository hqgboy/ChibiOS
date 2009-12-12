/*
    ChibiOS/RT - Copyright (C) 2006-2007 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ch.h"
#include "hal.h"
#include "test.h"
#include "evtimer.h"

#include "mmcsd.h"
#include "buzzer.h"

#define BOTH_BUTTONS (PAL_PORT_BIT(PA_BUTTON1) | PAL_PORT_BIT(PA_BUTTON2))

/*
 * Red LEDs blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

  (void)arg;
  while (TRUE) {
    palClearPort(IOPORT1, PAL_PORT_BIT(PA_LED2));
    chThdSleepMilliseconds(200);
    palSetPort(IOPORT1, PAL_PORT_BIT(PA_LED1) | PAL_PORT_BIT(PA_LED2));
    chThdSleepMilliseconds(800);
    palClearPort(IOPORT1, PAL_PORT_BIT(PA_LED1));
    chThdSleepMilliseconds(200);
    palSetPort(IOPORT1, PAL_PORT_BIT(PA_LED1) | PAL_PORT_BIT(PA_LED2));
    chThdSleepMilliseconds(800);
  }
  return 0;
}

/*
 * Yellow LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread2, 128);
static msg_t Thread2(void *arg) {

  (void)arg;
  while (TRUE) {
    palClearPad(IOPORT1, PA_LEDUSB);
    chThdSleepMilliseconds(200);
    palSetPad(IOPORT1, PA_LEDUSB);
    chThdSleepMilliseconds(300);
  }
  return 0;
}

static WORKING_AREA(waTestThread, 128);

/*
 * Executed as event handler at 500mS intervals.
 */
static void TimerHandler(eventid_t id) {

  (void)id;
  if (!(palReadPort(IOPORT1) & BOTH_BUTTONS)) {
    Thread *tp = chThdCreateStatic(waTestThread, sizeof(waTestThread),
                                   NORMALPRIO, TestThread, &SD1);
    chThdWait(tp);
    PlaySound(500, MS2ST(100));
  }
  else {
    if (!palReadPad(IOPORT1, PA_BUTTON1))
      PlaySound(1000, MS2ST(100));
    if (!palReadPad(IOPORT1, PA_BUTTON2)) {
      sdWrite(&SD1, (uint8_t *)"Hello World!\r\n", 14);
      PlaySound(2000, MS2ST(100));
    }
  }
}

/*
 * Plays sounds when a MMC/SD card is inserted, then initializes the MMC
 * driver and reads a sector.
 */
static void InsertHandler(eventid_t id) {
  static uint8_t rwbuf[512];
  MMCCSD data;

  (void)id;
  PlaySoundWait(1000, MS2ST(100));
  PlaySoundWait(2000, MS2ST(100));
  if (mmcInit())
    return;
  /* Card ready, do stuff.*/
  if (mmcGetSize(&data))
    return;
  if (mmcRead(rwbuf, 0))
    return;
  PlaySound(440, MS2ST(200));
}

/*
 * Plays sounds when a MMC/SD card is removed.
 */
static void RemoveHandler(eventid_t id) {

  (void)id;
  PlaySoundWait(2000, MS2ST(100));
  PlaySoundWait(1000, MS2ST(100));
}

/*
 * Entry point, note, the main() function is already a thread in the system
 * on entry.
 */
int main(int argc, char **argv) {
  static const evhandler_t evhndl[] = {
    TimerHandler,
    InsertHandler,
    RemoveHandler
  };
  static EvTimer evt;
  struct EventListener el0, el1, el2;

  (void)argc;
  (void)argv;

  /*
   * Activates the serial driver 2 using the driver default configuration.
   */
  sdStart(&SD1, NULL);

  /*
   * If a button is pressed during the reset then the blinking leds threads
   * are not started in order to make accurate benchmarks.
   */
  if ((palReadPort(IOPORT1) & BOTH_BUTTONS) == BOTH_BUTTONS) {
    chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);
    chThdCreateStatic(waThread2, sizeof(waThread2), NORMALPRIO, Thread2, NULL);
  }

  /*
   * Normal main() activity, in this demo it serves events generated by
   * various sources.
   */
  evtInit(&evt, MS2ST(500));            /* Initializes an event timer object.   */
  evtStart(&evt);                       /* Starts the event timer.              */
  chEvtRegister(&evt.et_es, &el0, 0);   /* Registers on the timer event source. */
  mmcStartPolling();                    /* Starts the MMC connector polling.    */
  chEvtRegister(&MMCInsertEventSource, &el1, 1);
  chEvtRegister(&MMCRemoveEventSource, &el2, 2);
  while (TRUE)                          /* Just serve events.                   */
    chEvtDispatch(evhndl, chEvtWaitOne(ALL_EVENTS));
  return 0;
}
