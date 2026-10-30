/* -----------------------------------------------------------------------------
 *
 * Clean stop on SIGINT/SIGTERM (Unix) or Ctrl+C, Ctrl+Break and closing the console (Windows).
 *
 * The handler only sets a flag. The main loop checks it between operations and leaves the way
 * the "quit" command does; an operation in progress lets the queries already sent finish, stops
 * sending new ones and fails the way a lost connection does, so the next run picks up from the
 * saved state. A second Ctrl+C (or SIGINT/SIGTERM) while stopping ends the process at once.
 * Same zlib license as the rest of BrickSync.
 *
 * -----------------------------------------------------------------------------
 */

#ifndef BSSTOP_H
#define BSSTOP_H

/* Install the handlers */
void bsStopInit( void );

/* Has a stop been requested? */
int bsStopRequested( void );

/* What asked for the stop, for messages: "SIGTERM", "Ctrl+C"... "" when none */
const char *bsStopSourceName( void );

/* The main loop has finished its clean exit (Windows: lets a console-close handler return early) */
void bsStopFinished( void );

#if CC_WINDOWS
/* The console handler's decision, without the wait done for a console close: return 1 when
   the event is handled (stop requested), 0 to let Windows end the process. For tests. */
int bsStopHandleConsoleEvent( unsigned long ctrltype );
#endif

#endif
