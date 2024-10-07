#pragma once
#define CLOCK_H

class Clock {
	
public:
	char cpu_clock;//clock for CPU ticks when counter hits predtermined value (eg 3 from 1)
	bool cycle_engaged;// TRUE if TIA or CPU are currently processing
	bool sim_paused; // used to signify that the console is currently waiting on new frame for the simulation to continue
	Clock();//resetting clock to 0
	void cycle_end(); // end of clock cycle, may run TIA and CPU from this module
	void tick(); //system clock tick, may run from here

};

