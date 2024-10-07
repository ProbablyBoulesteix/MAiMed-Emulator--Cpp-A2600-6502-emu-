#include <iostream>
#include "timer.h"

Clock::Clock() {
	cpu_clock = 0; //reset
	cycle_engaged = 0; //no current cycle on startup 
}

void Clock::cycle_end() {
	cycle_engaged = 0; //done with current cycle
}

void Clock::tick() {
	cycle_engaged = 1;
	cpu_clock = cpu_clock + 1; //updated system counter
	if (cpu_clock == 3) { //time for CPU to run !
		//cpu processing routine TBD
		cpu_clock = 0; //resetting delay
	}
	// TIA video processing routine
}	//update video
	//generate frame