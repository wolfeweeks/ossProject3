/**
 * @file worker.c
 * @author Wolfe Weeks
 * @date 2023-02-28
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include "shared_memory.h"

int* block; //shared memory block

// signal handler function to detach shared memory block and exit the program on receiving SIGPROF, SIGTERM, or SIGINT
static void myhandler(int s) {
  if (s == SIGPROF || s == SIGTERM) {
    detach_memory_block(block);
    exit(-1);
  } else if (s == SIGINT) {
    detach_memory_block(block);
    exit(-1);
  }
}

// function to set up signal handler function for SIGPROF
static int setupinterrupt(void) {
  struct sigaction act;
  act.sa_handler = myhandler;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

// function to print termination message and detach shared memory block
void terminate(int* block, int clockSec, int clockNano, int quitSec, int quitNano) {
  printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", getpid(), getppid(), clockSec, clockNano, quitSec, quitNano);
  printf("--Terminating\n");
  detach_memory_block(block);
  exit(1);
}

int main(int argc, char* argv[]) {

  // set up signal handler for SIGPROF
  if (setupinterrupt() == -1) {
    printf("Failed to set up handler for SIGPROF\n");
    exit(-1);
  }

  // check if command line arguments are passed
  if (argc != 3) {
    printf("Error: missing command line argument for time limit\n");
    return 1;
  }

  // convert command line arguments to integers
  int limit[] = { atoi(argv[1]) , atoi(argv[2]) };

  // attach to the shared memory clock initialized in oss.c
  block = attach_memory_block("README.txt", sizeof(int) * 2);
  if (block == NULL) {
    printf("ERROR: couldn't get block\n");
    exit(1);
  }

  // store the contents of shared memory into clock array
  int clock[2];
  memcpy(clock, block, sizeof(int) * 2);

  int quitTime[2] = { limit[0] + clock[0], limit[1] + clock[1] };
  if (quitTime[1] >= 1000000000) { //check if nano seconds exceed 1 second
    quitTime[0] += 1;
    quitTime[1] -= 1000000000;
  }

  printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", getpid(), getppid(), clock[0], clock[1], quitTime[0], quitTime[1]);
  printf("--Just Starting\n");

  // Initialize previous seconds and elapsed seconds
  int prevSeconds = clock[0];
  int elapsedSeconds = 0;

  // Loop indefinitely
  while (true) {

    // Get current clock value from shared memory block
    memcpy(clock, block, sizeof(int) * 2);

    // Check if current seconds is greater than termination seconds
    if (clock[0] > quitTime[0])
      terminate(block, clock[0], clock[1], quitTime[0], quitTime[1]);

    // Check if current seconds is equal to termination seconds and current nanoseconds is greater than or equal to termination nanoseconds
    if (clock[0] == quitTime[0] && clock[1] >= quitTime[1])
      terminate(block, clock[0], clock[1], quitTime[0], quitTime[1]);

    // Check if current seconds has changed since last loop iteration
    if (clock[0] != prevSeconds) {
      // Update previous seconds and elapsed seconds
      prevSeconds = clock[0];
      elapsedSeconds += 1;

      // Print worker information and elapsed time
      printf("WORKER PID:%d PPID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", getpid(), getppid(), clock[0], clock[1], quitTime[0], quitTime[1]);
      printf("--%d seconds have passed since starting\n", elapsedSeconds);
    }
  }


  return 0;
}
