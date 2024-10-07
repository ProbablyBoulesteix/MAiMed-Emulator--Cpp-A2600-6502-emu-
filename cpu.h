

#pragma once
#define CPU_H

#include "memory.h"


class Processor {
public:
	//Internal state modelling
	int step; //counting the cycle on which the CPU is currently on
	bool waiting; //flag, set to TRUE if processor is waiting (ex: RDY pin asserted by TIA)
	//constructor
	Processor(MemIO mem); 
	//main methods
	void cpu_tick(MemIO mem); //run one clock cycle of the 6502 processor
	void sleep(); //RDY pin asserted
	void wake(); //RDY pin unasserted, eg Hblank.
	void reset(MemIO mem); //resetting
	void dump_registers(); //prints register contents
	
	//registers
	unsigned char A; //accumulator
	unsigned char X;
	unsigned char Y;
	unsigned char SP; //stack pointer
	unsigned short PC; // Program counter;
	//flag registers
	bool C; //M0 Carry flag
	bool Z; //M1 Zero flag
	bool I; //M2 Interrupt flag (disabled on 6507)
	bool D; //M3 Decimal/BCD flag (disabled on 6507?)
	bool B_h; //M4/M5 Break flag, ???
	bool B_l; //M4/M5 Break flag, ???
	bool V; //M6 Overflow flag
	bool N; // M7Negative flag
private:
	//private methods within compute loop
	void compute(unsigned char opcode, MemIO mem); //execute the operation. Currenty public for initial debugging, set to private once done!
	void wait();
	

};





