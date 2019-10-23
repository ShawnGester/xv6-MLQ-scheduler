#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main (int argc, char* argv[]) {
	// int timerTicks;
	// int numIteration;
	// int jobCount = 0;
	
	// if (argc != 5) {
	// 	return -1;
	// }
	// if(argv[1] < 0) {
	// }
	
	// else {
	// 	timerTicks = atoi(argv[1]);
	// }
	// if(argv[2] < 0) {
	// }
	// else {
	// 	numIteration = atoi(argv[2]);
	// }
	// if(argv[4] < 0) {
	// }
	// else{
	// 	jobCount = atoi(argv[4]);
	// }

	// int pids[jobCount];
	// fork2(3); //FIXME:
	// for (int i = 0; i < numIteration; ++i) {
		
	// 	int pid = fork2(0);
	// 	if (pid != 0) {	// parent process
	// 		sleep(timerTicks);
	// 		setpri(
	// 	}
	// 	else {	// child process
	// 		for (int x = 0; x < jobCount; ++x) {
	// 			// strtok the 4th param
	// 			// exec()
				
	// 		}
	// 	}
	// }	
	/*int userTimeSlice = atoi(argv[1]);
	int iterations = atoi(argv[2]);
	int jobCount = atoi(argv[argc-1]);
	int[] // store all pids
	if (strncmp("loop", argv[3], 4) == 0) {
		int pid = fork();
		for (int i = 0; i < userTimeSlice; ++i) {
			int pid = fork();
			if (pid != 0) {	// parent
				sleep(1);	// sleep for 10 ms
				printf(1, "%d\n", pid);
			} else {	// child
				exit();
			}
		}
	}*/
	exit();
}
