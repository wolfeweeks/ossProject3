/**
 * @file oss.c
 * @author Wolfe Weeks
 * @date 2023-02-28
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

#include "shared_memory.h"

struct PCB {
  int occupied; // either true or false
  pid_t pid; // process id of this child
  int startSeconds; // time when it was forked
  int startNano; // time when it was forked
};

int proc;
struct PCB processTable[20];
int* block;

// This function is a signal handler that handles three different signals: SIGPROF, SIGTERM, and SIGINT.
static void myhandler(int s) {
  // If a SIGPROF or SIGTERM signal is received, terminate all running processes and exit the program.
  if (s == SIGPROF || s == SIGTERM) {
    printf("Timeout or termination signal received, terminating...\n");

    // Loop through the process table and send the signal to each process that is currently running.
    int i;
    for (i = 0; i < 20; i++) {
      if (processTable[i].occupied == true) {
        kill(processTable[i].pid, s);
      }
    }

    // Destroy the memory block and exit with an error code.
    destroy_memory_block("README.txt");
    detach_memory_block(block);
    exit(-1);
  }
  // If a SIGINT signal is received, terminate all running processes and exit the program.
  else if (s == SIGINT) {
    printf("Ctrl-c signal received, cleaning up and terminating...\n");

    // Loop through the process table and send the signal to each process that is currently running.
    int i;
    for (i = 0; i < 20; i++) {
      if (processTable[i].occupied) {
        kill(processTable[i].pid, s);
      }
    }

    // Destroy the memory block and exit with an error code.
    destroy_memory_block("README.txt");
    detach_memory_block(block);
    exit(-1);
  }
}

static int setupinterrupt(void) { /* set up myhandler for SIGPROF */
  struct sigaction act;
  act.sa_handler = myhandler;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

static int setupitimer(void) { /* set ITIMER_PROF for 2-second intervals */
  struct itimerval value;
  value.it_interval.tv_sec = 60;
  value.it_interval.tv_usec = 0;
  value.it_value = value.it_interval;
  return (setitimer(ITIMER_PROF, &value, NULL));
}

// This function increments the simulated system clock by 500 nanoseconds.
void incrementClock(int* clock, int* block) {
  // Add 500 nanoseconds to the second element of the clock array.
  clock[1] += 500;

  // If the second element of the clock array has exceeded or equalled 1 billion nanoseconds,
  // increment the first element of the clock array by 1 and subtract 1 billion nanoseconds from the second element.
  if (clock[1] >= 1000000000) {
    clock[0] += 1;
    clock[1] -= 1000000000;
  }

  // Copy the contents of the clock array to the memory block.
  memcpy(block, clock, sizeof(int) * 2);
}

void printPCBTable(int* clock) {
  printf("OSS PID:%d SysClockS:%d SysClockNano:%d\n", getpid(), clock[0], clock[1]);
  printf("Process Table:\n");
  printf("|%10s |%10s |%15s |%15s |\n", "Occupied", "PID", "Start Seconds", "Start Nano");
  printf("|%10s |%10s |%15s |%15s |\n", "----------", "----------", "---------------", "---------------");

  int i;
  for (i = 0; i < 20; i++) {
    printf("|%10s |%10d |%15d |%15d |\n", processTable[i].occupied ? "true" : "false", processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
  }
}

int main(int argc, char* argv[]) {
  if (setupinterrupt() == -1) {
    printf("Failed to set up handler for SIGPROF\n");
    exit(-1);
  }
  if (setupitimer() == -1) {
    printf("Failed to set up 60s timer\n");
    exit(-1);
  }

  // Initialize variables to hold command line options
  int proc = 1, simul = 1, limit = 5;

  // set occupied values in process control block array to false
  int i;
  for (i = 0; i < proc; i++) {
    processTable[i].occupied = false;
  }

  // Use getopt() function to parse command line options
  int opt;
  while ((opt = getopt(argc, argv, "n:s:t:h")) != -1) {
    switch (opt) {
    case 'n':
      proc = atoi(optarg);
      break;
    case 's':
      simul = atoi(optarg);
      break;
    case 't':
      limit = atoi(optarg);
      break;
    case 'h':
      printf("Usage: oss [-h] [-n proc] [-s simul] [-t iter]\n");
      printf("\t-h (optional) shows a help message\n");
      printf("\t-n (optional) is the total number of child processes to create\n");
      printf("\t-s (optional) is the maximum number of concurrent child processes\n");
      printf("\t-t (optional) is the maximum number of seconds each child process should run for\n");
      return 0;
    default:
      printf("Invalid option\n");
      return 1;
    }
  }

  // initialize the clock that will be put in shared memory
  int clock[] = { 0, 0 };

  // get shared memory block for clock
  block = attach_memory_block("README.txt", sizeof(int) * 2);

  // If block is NULL, it means that shared memory block for clock couldn't be obtained.
  if (block == NULL) {
    printf("ERROR: couldn't get shared memory block\n");
    exit(1);
  }

  // Copy the contents of the clock array to the memory block.
  memcpy(block, clock, sizeof(int) * 2);

  int remaining = proc;
  int running = 0;

  int prevClock[] = { 0, 0 };

  while (1) {
    // Increment the simulated system clock by 500 nanoseconds.
    incrementClock(clock, block);

    // If a second has passed, print the Process Control Block table.
    if (clock[0] - prevClock[0] >= 1) {
      printPCBTable(clock);
      prevClock[0] = clock[0];
      prevClock[1] = clock[1];
    }
    // If 500 million nanoseconds have passed since the last print, print the Process Control Block table.
    else if (clock[0] == prevClock[0] && clock[1] - prevClock[1] >= 500000000) {
      printPCBTable(clock);
      prevClock[0] = clock[0];
      prevClock[1] = clock[1];
    }

    // If the number of running processes is less than the limit and there are remaining processes to run.
    if (running < simul && remaining > 0) {
      // Fork a new process.
      pid_t pid = fork();

      // If fork failed.
      if (pid < 0) {
        printf("Fork failed!\n");
        exit(1);
      }
      // If this is the child process.
      else if (pid == 0) {
        // Update the process table for the child process.
        processTable[proc - remaining].pid = pid;
        processTable[proc - remaining].occupied = true;
        processTable[proc - remaining].startSeconds = clock[0];
        processTable[proc - remaining].startNano = clock[1];

        // Seed the random number generator with the time and the process ID.
        srand(time(0) + getpid());

        // Generate random seconds and nanoseconds.
        int randSeconds = (rand() % (limit + 1));
        int randNano;
        if (randSeconds < limit) {
          randNano = (rand() % 1000000001);
        } else {
          randNano = 0;
        }

        // Convert random seconds and nanoseconds to strings.
        char secStr[2];
        char nanoStr[10];

        sprintf(secStr, "%d", randSeconds);
        sprintf(nanoStr, "%d", randNano);

        // Execute the worker program with the random seconds and nanoseconds as arguments.
        execl("./worker", "./worker", secStr, nanoStr, NULL);
      }
      // If this is the parent process.
      else {
        // Update the process table for the parent process.
        processTable[proc - remaining].pid = pid;
        processTable[proc - remaining].occupied = true;
        processTable[proc - remaining].startSeconds = clock[0];
        processTable[proc - remaining].startNano = clock[1];

        // Increment the number of running processes and decrement the number of remaining processes.
        running++;
        remaining--;
      }
    }
    // If no more processes to run or running processes exit while loop
    else if (remaining == 0 && running == 0) {
      break;
    }
    // If the maximum number of simultaneous processes are running wait for a child to die
    else {
      int res = waitpid(-1, NULL, WNOHANG);
      if (res > 0) {
        running--;
      }
    }
  }

  if (!destroy_memory_block("README.txt")) {
    printf("ERROR: Couldn't destroy shared memory block\n");
  }

  detach_memory_block(block);

  return 0;
}
