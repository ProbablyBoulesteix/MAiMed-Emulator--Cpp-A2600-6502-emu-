#include <iostream>

#include "cpu.h"




// #### RANDOM USEFUL FUNCTIONS ####
bool isNegative_8b(unsigned char value) {
	char x = value;
	return (x < 0);
}

unsigned short concatenate2x8b(unsigned char low_b, unsigned char high_b) {
	unsigned short res = (high_b << 8) | low_b;
	return res;
}

unsigned char pack_SR(bool N, bool V, bool B_h, bool B_l, bool D, bool I, bool Z, bool C) {
	unsigned char packed_SR = (N << 7) | (V << 6) | (B_h << 5) | (B_l << 4) | (D << 3) | (I << 2) | (Z << 1) | C; //packing all SR bits into single uchar
	return packed_SR;
}

bool crossing_page_boundary(unsigned short add, unsigned char offset, bool carry = 0) {//determines if 16 bit + 8 bit addition changes upper byte of 16 bit result or not. 6502 sometimes takes an extra cycle if page boundaries are crossed
	unsigned char upper = add >> 8; // upper 8 bits of address, should remain constant on single page
	unsigned short res = add + offset + carry;
	unsigned char res_upper = res >> 8; //upper 8 btis of addition
	if (res_upper == upper) { return 0; }//page not crossed, upper bits stayed the same
	else { return 1; } //page crossed, may need extra cycle
}

bool crossing_page_notconcatenated(unsigned char low, unsigned char high, unsigned char offset, bool carry = 0) {//determines if 16 bit + 8 bit addition changes upper byte of 16 bit result or not. 6502 sometimes takes an extra cycle if page boundaries are crossed
	unsigned short add = concatenate2x8b(low, high); //get proper 16bit address
	unsigned char upper = add >> 8; // upper 8 bits of address, should remain constant on single page
	unsigned short res = add + offset + carry;
	unsigned char res_upper = res >> 8; //upper 8 btis of addition
	if (res_upper == upper) { return 0; }//page not crossed, upper bits stayed the same
	else { return 1; } //page crossed, may need extra cycle
}

bool crossing_page_jump(unsigned short PC, char offset) { //checks to see if jump target is outside of current page
	unsigned char upper = PC >> 8;
	unsigned short res = PC + offset;
	unsigned char res_upper = res >> 8; //upper byte of target PC
	if (res_upper == upper) { return 0; }//target PC is still on current page
	else { return 1; } //page boundary crossed
}

// ##### MAIN CPU LOOP ######

void Processor::cpu_tick(MemIO mem) { //main loop for CPU
	// at beginning of cycle, 6502 state is defined
	unsigned char opcode = mem.read(PC); //get current opcode from current PC
	if (waiting == 0) { //if processor is currently active (not RDY state)
		if (step > 0) { //cpu currently doing something, wait to next cycle. Using > instead of != in case counter somehow goes under 0
			wait();
		}
		else { //main processing loop, CPU starts up a new instruction
			printf("Opcode: %x ; PC: %x ; step: %d ; registers A:%d; X:%d; Y:%d ; flags N:%d Z:%d, C:%d, I:%d, D:%d, V:%d, SP: %x \n", opcode, PC, step, A, X, Y, N, Z, C, I, D, V, SP);
			compute(opcode, mem); //perform the instruction and modify state
		}
	}
	else { //RDY state

	}


}

// ####### aux methods for CPU #####

Processor::Processor(MemIO mem) { //constructor
	Processor::reset(mem);//reset at system startup
}

void Processor::reset(MemIO mem) { //reset state to startup
	//initialising registers
	A = 0;
	X = 0;
	Y = 0;
	SP = 0xFF; //top of stack, need to check if this is a system default or just common practice
	//PC fetches from top of reset vectors at $FFFC/$FFFD
	unsigned char low = mem.read(0xFFFC);
	unsigned char high = mem.read(0xFFFD);
	//PC = concatenate2x8b(low, high); //place to start from at system reset
	//printf("%x \n", PC);
	PC = 0xF000; //pending 
	D = 0;
	Z = 0;
	C = 1;
	N = 0;
	V = 0;
	I = 1;
	//resetting program flags
	step = 0; //currently no in-progress operations
	waiting = 0;
}

void Processor::wait() { //new system clock tick, instruction has yet to finish on 6502: stay idle on simulated CPU
	step = step - 1; //advancing one clock tick
}


void Processor::compute(unsigned char opcode, MemIO mem){ // we have a valid opcode and the processor is expected to work (last clock tick)
	//value declaration
	unsigned char imm; // an operand to an instruction
	unsigned char low, high; // the lower-most and upper-most bytes of a 16-bit value (usually an address)
	unsigned short val_ptr, add_ptr; // resp: 16 pointer to the operand (imm) and pointer to said pointer (for indirect addressing modes)
	unsigned char val_ptr_zp, add_ptr_zp; //8-bit pointer used for zeropage operations. We specifically WANT an overflow to happen if $P + X > 255, so we're not using a short like with the other pointer
	unsigned int Ap; // temporary storage space for arithmetic operation results where overflow won't happen. If overflow happens when carrying over to 8 bits the value of A and Ap will be different
	char offset; // an 8bit signed offset for use by relative-mode instructions

	unsigned char SR_backup; //place to store the packed flag bits of the 6502 status register
	unsigned short PC_backup; //place to store a current/future PC value for use in instruction such as BRK

	bool PC_cross_PB, Cp; //flag if PC has crossed page boundary post jump, temporary carry flag when needing to store pre-instruction state
	switch (opcode) {
	/*########################### Add with Carry(ADC) ######################################## */

	case 0x69: //ADC immediate, 2b 2c
		step = 2; //setting cycle length
		imm = mem.read(PC + 1); //the instruction operand immediately after opc
		//main artihmetic block
		Ap = A + imm + C; // doing operation without risk of overflow
		A = Ap; // casting uint to uchar, may overflow. Different values if overflow.
		//updating flags
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes, procede to next step
		PC = PC + 2;
		step = step - 1;
		break; //done
	
	case 0x65: //ADC zeropage, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1); //address (8b) of operand 
		imm = mem.read(val_ptr_zp);
		Ap = A + imm + C; //placeholder value before overflow
		A = Ap; //casting to word size
		//updating flags;
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes
		PC = PC + 2;
		step = step - 1; 
		break; 

	case 0x75: //ADC zeropage X, 2b 4c;
		step = 4;
		//fetching operand
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp);
		//main arithmetic
		Ap = A + imm + C;
		A = Ap;
		//flags
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes
		PC = PC + 2;
		step = step - 1;
		break;
	
	case 0x6D: // ADC absolute, 3b 4c
		step = 4;
		//fetching high and low address bytes
		low = mem.read(PC + 1); //Little endian !
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		//doing arithmetic
		Ap = A + imm + C;
		A = Ap;
		//flags
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x7D: //ADC absolute,X 3b 4c+
		step = 4; //may take more, ignoring for now;
		low = mem.read(PC + 1); //Little endian !
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X;
		imm = mem.read(val_ptr);
		//doing arithmetic
		Ap = A + imm + C;
		A = Ap;
		//flags
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;
	
	case 0x79: //ADC absolute, Y, 3b 4c;
		step = 4; //may take more, ignoring for now;
		low = mem.read(PC + 1); //Little endian !
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + Y;
		imm = mem.read(val_ptr);
		//doing arithmetic
		Ap = A + imm + C;
		A = Ap;
		//flags
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	case 0x61: // ADC pre-indirect ZP, X, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X; //address to value pointer bytes
		low = mem.read(add_ptr_zp); // low byte of value pointer (LE!)
		high = mem.read(add_ptr_zp+1); //high byte of value pointer (LE!)
		val_ptr = concatenate2x8b(low, high); //full address of operand
		imm = mem.read(val_ptr); //operand
		//doing arithmetic
		Ap = A + imm + C;
		A = Ap;
		//flags
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes
		PC = PC + 2;
		step = step - 1;
		break;
	
	case 0x71: //ADC post-indirect ZP, Y, 2b 5c+
		step = 5;
		add_ptr_zp = mem.read(PC + 1); //address in zero page containing pointer
		low = mem.read(add_ptr_zp); // low byte of value pointer at previously determined address
		high = mem.read(add_ptr_zp + 1); //high byte of value pointer right after low byte in memory
		val_ptr = concatenate2x8b(low, high) + Y; //final address of operand
		imm = mem.read(val_ptr);
		//doing arithmetic
		Ap = A + imm + C;
		A = Ap;
		//flags
		N = isNegative_8b(A);
		Z = (A == 0); //checking if new A value is zero
		C = (A != Ap); // if sum of two 8bit nbrs does not fit in 8 bits, the actual result will be different from the "real" mathematical result (contained in int)
		V = (isNegative_8b(A) && isNegative_8b(imm)); //signed overflow happens if two numbers have same sign bit. 
		//instruction finishes
		PC = PC + 2;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	/*################################ AND Memory with Accumulator (AND) #####################*/

	case 0x29: // AND imm, 2b 2c
		step = 2;
		// logic ops
		imm = mem.read(PC + 1); //getting value from memory
		A = A & imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0); 
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x25: //AND Zeropage, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp);
		A = A & imm; 
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x35: //AND Zeropage X, 2b 4c
		step = 4;
		//getting values from memory
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp);
		A = A & imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x2D: //AND absolute 3b 4c
		step = 4;
		low = mem.read(PC + 1); //getting low byte of address in instruction
		high = mem.read(PC + 2); //getting high byte
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		//op
		A = A & imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x3D: //AND absolute X, 3b 4c+
		step = 4;
		low = mem.read(PC + 1); //getting low byte of address in instruction
		high = mem.read(PC + 2); //getting high byte
		val_ptr = concatenate2x8b(low, high) + X;
		imm = mem.read(val_ptr);
		//op
		A = A & imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;

	case 0x39: // AND absolute Y, 3b 4c+
		step = 4;
		low = mem.read(PC + 1); //getting low byte of address in instruction
		high = mem.read(PC + 2); //getting high byte
		val_ptr = concatenate2x8b(low, high) + Y;
		imm = mem.read(val_ptr);
		//op
		A = A & imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	case 0x21: //AND pre-indirect X, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X; //address of pointer to operand
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		//op
		A = A & imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x31: // AND post-indirect X 2b 5c+
		step = 5;
		add_ptr_zp = mem.read(PC + 1); //address of pointer in ZP and instruction
		//getting contents of last pointer
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high) + Y; // pointer to value with offset
		imm = mem.read(val_ptr);
		//op
		A = A & imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 2;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	/* ############################ Shift left one bit (ASL) ###############################*/

	case 0x0A: // ASL Acc, 1b 2c
		step = 2;
		C = A & 0x80; // keeping the last bit for the carry flag (hint: 0x80 is binary 1000 0000)
		A = A << 1; //new result
		//other flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 1;
		step = step - 1;
		break;

	case 0x06: //ASL zeropage, 2b 5c;
		step = 5;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp);
		C = imm & 0x80; //keeping MSB
		//op
		imm = imm << 1;
		//other flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//writing back to source memory address
		mem.write(val_ptr_zp, imm); //writeback complete !
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x16: //ASL zeropage X, 2b 6c;
		step = 6;
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp);
		C = imm & 0x80; //keeping MSB
		//op
		imm = imm << 1;
		//other flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//writing back to source memory address
		mem.write(val_ptr_zp, imm); //writeback complete !
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x0E: // ASL absolute, 3b 6c
		step = 6;
		//getting address of operand
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		//operations
		C = imm & 0x80; //keeping MSB
		imm = imm << 1; 
		//other flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//writing back to source memory address
		mem.write(val_ptr, imm); //writeback complete !
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x1E: // ASL absolute X, 3b 7c
		step = 7;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X;
		imm = mem.read(val_ptr);
		//operations
		C = imm & 0x80; //keeping MSB
		imm = imm << 1;
		//other flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//writing back to source memory address
		mem.write(val_ptr, imm); //writeback complete !
		PC = PC + 3;
		step = step - 1;
		break;

	/* ########### Branch on carry clear (BCC) #############*/

	case 0x90: // BCC relative, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1);
		if (C == 0) {
			PC_cross_PB = crossing_page_jump(PC, offset);
			// calculating new target address
			PC = PC + offset + 2; //jumping to target

		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB; //if original PC + target  jumps to other page, extra cycle taken
		break;
	
	/* ########### Branch on carry set (BCS) #############*/

	case 0xB0: // BCS relative, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1);
		if (C == 1) {
			PC_cross_PB = crossing_page_jump(PC, offset);
			// calculating new target address
			PC = PC + offset +2; //jumping to target
		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB; //if original PC + target  jumps to other page, extra cycle taken
		break;

	/* ###### Branch on result Zero (BEQ) ########*/

	case 0xF0: //BEQ relative, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1);
		if (Z == 1) { //previous result is zero, branch
			PC_cross_PB = crossing_page_jump(PC, offset);
			PC = PC + offset + 2;
		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB; //if original PC + target  jumps to other page, extra cycle taken
		break;

	/* ######## Test Bits in Memory with accumulator (BIT) #############*/

	case 0x24: // BIT zeropage, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp);
		//updating flags
		N = A & 0x80;
		V = A & 0x40;
		Z = A && imm;
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x2C: //BIT absolute, 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); //address of operand
		imm = mem.read(val_ptr);
		//updating flags
		N = A & 0x80;
		V = A & 0x40;
		Z = A && imm;
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	/* #################### Branch on result Minus (BMI) ########## */
	
	case 0x30: //BMI relative, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1);
		if (N == 1) { //if previous result is negative
			PC_cross_PB = crossing_page_jump(PC, offset);
			PC = PC + offset + 2;
			
		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB;
		break;

	/* ######## Branch on result not Zero (BNE) #######*/

	case 0xD0: // BNE relative, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1);
		if (Z == 0) { //result not zero
			PC_cross_PB = crossing_page_jump(PC, offset);
			PC = PC + offset +2;
		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB;
		break;

	/* ####### Branch on result Plus (BPL) #########*/

	case 0x10: // BPL res, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1);
		if (N == 0) {// result is positive
			PC_cross_PB = crossing_page_jump(PC, offset);
			PC = PC + offset + 2;
		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB;
		break;
	/* ######## Force Break (BRK) #######*/
	
	case 0x00: // BRK, 1b 7c 
		step = 7;
		//keeping record of current SR and PC before stack push
		SR_backup = pack_SR(N, V, B_h, B_l, D, I, Z, C);
		PC_backup = PC + 2;
		low = PC_backup; //short to char cast gets rid of upper byte
		high = (PC_backup >> 8); //keep topmost byte
		//setting flags
		I = 1;	
		B_l = 1;
		//pushing PC+2 to stack
		mem.write(SP, high); // stored first byte
		SP = SP - 1; //decrementing stack
		mem.write(SP, low); //stored upper byte
		SP = SP - 1; //updating stack pointer to reflect new stack "top"
		//pushing SR to stack
		mem.write(SP, SR_backup);
		SP = SP - 1; //pushed SR to stack
		//fetching new PC, stored at addresses $FFFE and $FFFF
		low = mem.read(0xFFFE);
		high = mem.read(0xFFFF);
		PC = concatenate2x8b(low, high); //new PC to run from

	/* ###### Branch on Overflow Clear (BVC) #######*/

	case 0x50: // BVC relative, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1); //getting offset from instruction
		if (V == 0) { //if overcflow clear
			PC_cross_PB = crossing_page_jump(PC, offset);
			PC = PC + offset + 2;
		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB;
		break;

		/* ###### Branch on Overflow set (BVS) #######*/

	case 0x70: // BVS relative, 2b 2c++
		step = 2;
		offset = mem.read(PC + 1); //getting offset from instruction
		if (V == 1) { //if overcflow set
			PC_cross_PB = crossing_page_jump(PC, offset);
			PC = PC + offset + 2;
		}
		else {
			PC = PC + 2;
			PC_cross_PB = 0;
		}
		step = step - 1 + PC_cross_PB;
		break;
	
	/* ########### Clear Carry Flag (CLC) ########*/

	case 0x18: // CLC 1b 2c
		step = 2;
		C = 0; //clearing flag
		PC = PC + 1;
		step = step - 1;
		break;

	/* ########### Clear decimal Flag (CLD) ########*/

	case 0xD8: // CLD 1b 2c
		step = 2;
		D = 0; //clearing flag
		PC = PC + 1;
		step = step - 1;
		break;

	/* ########### Clear interrupt disable bit (CLI) ########*/

	case 0x58: // CLI 1b 2c
		step = 2;
		I = 0; //clearing flag
		PC = PC + 1;
		step = step - 1;
		break;

		/* ########### Clear overflow Flag (CLV) ########*/

	case 0xB8: // CLV 1b 2c
		step = 2;
		V = 0; //clearing flag
		PC = PC + 1;
		step = step - 1;
		break;

	/* ###### Compare Memory with Accumulator (CMP) ######*/

	case 0xC9: // CMP immediate, 2b 2c
		step = 2;
		imm = mem.read(PC + 1);
		C = (A >= imm); //setting carry if A value is gretaer than memory contents
		imm = A - imm; //result of accumulator/memory compare
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xC5: // CMP zeropage 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp); //value to compare 
		C = (A >= imm);
		imm = A - imm; //doing the operation
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xD5: //  CMP zeropage X, 2b 4c
		step = 4;
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp); //value to compare 
		C = (A >= imm);
		imm = A - imm; //doing the operation
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xCD: //CMP absolute 3b 4c
		step = 3;
		//fetching address
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high);
		//fetching data
		imm = mem.read(val_ptr);
		C = (A >= imm); 
		imm = A - imm;
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 3;
		step = step - 1;
		break;

	case 0xDD: //CMP absolute X 3b 4c+
		step = 4;
		//fetching address
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X;
		//fetching data
		imm = mem.read(val_ptr);
		C = (A >= imm);
		imm = A - imm;
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;
	
	case 0xD9: //CMP absolute Y 3b 4c+
		step = 4;
		//fetching address
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + Y;
		//fetching data
		imm = mem.read(val_ptr);
		C = (A >= imm);
		imm = A - imm;
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	case 0xC1: //CMP pre-indexed X, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X;
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		C = (A >= imm); //setting flag
		imm = A - imm; // getting compare
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xD1: // CMP post-indexed Y, 2b 5c+
		step = 5;
		add_ptr_zp = mem.read(PC + 1); //zeropage address of pointer 
		low = mem.read(add_ptr_zp); // low byte of pointer address
		high = mem.read(add_ptr_zp + 1); //high """"
		val_ptr = concatenate2x8b(low, high) + Y; // real address of value zith added index
		//instruction body
		imm = mem.read(val_ptr);
		C = (A >= imm); //setting flag
		imm = A - imm; // getting compare
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	/* ################# Compare Memory and Index X (CPX) #######*/

	case 0xE0: //CPX imm, 2b 2c
		step = 2;
		imm = mem.read(PC + 1);
		C = (X >= imm); //setting carry if A value is greater than memory contents
		imm = X - imm; //result of accumulator/memory compare
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xE4: //CPX zp, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp); //value to compare 
		C = (X >= imm);
		imm = X - imm; //doing the operation
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xEC: // CPX abs, 3b 4c
		step = 3;
		//fetching address
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high);
		//fetching data
		imm = mem.read(val_ptr);
		C = (X >= imm);
		imm = X - imm;
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 3;
		step = step - 1;
		break;
		/* ########## Compare Memory with index Y (CPY) ########*/
	case 0xC0: //CPY imm, 2b 2c
		step = 2;
		imm = mem.read(PC + 1);
		C = (Y >= imm); //setting carry if A value is greater than memory contents
		imm = Y - imm; //result of accumulator/memory compare
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xC4: //CPY zp, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp); //value to compare 
		C = (Y >= imm);
		imm = Y - imm; //doing the operation
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xCC: // CPX abs, 3b 4c
		step = 3;
		//fetching address
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high);
		//fetching data
		imm = mem.read(val_ptr);
		C = (Y >= imm);
		imm = Y - imm;
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		PC = PC + 3;
		step = step - 1;
		break;

	/* ######## Decrement memory by one (DEC) #######*/

	case 0xC6: // DEC zeropage, 2b 5c
		step = 5;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp) - 1; // getting value from memory and decrementing
		//writing back to memory
		mem.write(val_ptr_zp, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xD6: // DEC zeropage X, 2b 6c
		step = 6;
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp) - 1; // getting value from memory and decrementing
		//writing back to memory
		mem.write(val_ptr_zp, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xCE: // DEC absolute, 3b 6c
		step = 6;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr) -1;// getting value and decrementing by 1
		//writing back
		mem.write(val_ptr, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0xDE: // DEC absolute X, 3b 7c
		step = 7;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X;
		imm = mem.read(val_ptr) - 1;// getting value and decrementing by 1
		//writing back
		mem.write(val_ptr, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 3;
		step = step - 1;
		break;
	
	/* ######### Decrement index X by one (DEX) ######*/

	case 0xCA: // DEX 1b 2c
		step = 2;
		X = X - 1; //decrementing X
		N = isNegative_8b(X);
		Z = (X == 0);
		//done
		step = step - 1;
		PC = PC + 1;
		break;

	/* ######### Decrement index Y by one (DEY) ######*/

	case 0x88: // DEY 1b 2c
		step = 2;
		Y = Y - 1; //decrementing Y
		N = isNegative_8b(Y);
		Z = (Y == 0);
		//done
		step = step - 1;
		PC = PC + 1;
		break;

	/* ######### Exclusive OR with accumulator (EOR)########*/

	case 0x49: //EOR immediate, 2b 2c
		step = 2;
		imm = mem.read(PC + 1);
		A = A ^ imm; //bitwise XOR
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x45: // EOR zeropage, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1); //address of operand in ZP
		imm = mem.read(val_ptr_zp);
		A = A ^ imm; //bitwise XOR
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x55: //EOR Zeropage X, 2b 4c
		step = 4;
		val_ptr_zp = mem.read(PC + 1) + X; //address of operand in ZP
		imm = mem.read(val_ptr_zp);
		A = A ^ imm; //bitwise XOR
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x4D: // EOR abs, 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); //address of operand
		imm = mem.read(val_ptr);
		A = A ^ imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x5D: // EOR abs X, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; //address of operand
		imm = mem.read(val_ptr);
		A = A ^ imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;

	case 0x59: // EOR abs Y, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + Y; //address of operand
		imm = mem.read(val_ptr);
		A = A ^ imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	case 0x41: // EOR pre-indirect X, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X;
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		A = A ^ imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x51: //EOR post-indirect Y, 2b 5c+
		step = 5;
		add_ptr_zp = mem.read(PC + 1);
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high) + Y;
		imm = mem.read(val_ptr);
		A = A ^ imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	/* ######### Increment memory by one (INC) ######*/

	case 0xE6: // INC Zeropage, 2b 5c
		step = 5;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp) + 1; //incremented value from ram
		//writing back to ram!
		mem.write(val_ptr_zp, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done 
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xF6: // INC zeropage X, 2b 6c
		step = 6;
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp) + 1; //incremented value from ram
		//writing back to ram!
		mem.write(val_ptr_zp, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done 
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xEE: // INC absolute, 3b 6c
		step = 6;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); //pointer to operand
		imm = mem.read(val_ptr) + 1; // incrementing memory value by 1
		//writing back !
		mem.write(val_ptr, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		step = step - 1;
		PC = PC + 3;
		break;

	case 0xFE: // INC absolute X, 3b 7c
		step = 7;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; //pointer to operand
		imm = mem.read(val_ptr) + 1; // incrementing memory value by 1
		//writing back !
		mem.write(val_ptr, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		step = step - 1;
		PC = PC + 3;
		break;

	/* ###### Increment X by one (INX) #####*/

	case 0xE8: //INX implied, 1b 2c
		step = 2;
		X = X + 1;
		//setting flags
		N = isNegative_8b(X);
		Z = (X == 0);
		//done
		step = step - 1;
		PC = PC + 1;
		break;

	/* ###### Increment Y by one (INY) #####*/

	case 0xC8: //INY implied, 1b 2c
		step = 2;
		Y = Y + 1;
		//setting flags
		N = isNegative_8b(Y);
		Z = (Y == 0);
		//done
		step = step - 1;
		PC = PC + 1;
		break;

	/* ################### Jump to new location (JMP) #######*/

	case 0x4C: // JMP absolute, 3b 3c
		step = 3;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high); //new target
		PC = add_ptr; // jumping
		step = step - 1;
		break;

	case 0x6C: // JMP indirect, 3b 5c
		step = 5;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high); // address of jump pointer target
		low = mem.read(add_ptr);
		high = mem.read(add_ptr + 1);
		add_ptr = concatenate2x8b(low, high); // address to jump to
		PC = add_ptr; //jump
		step = step - 1;
		break;

/* ################ Jump to new location, saving return address (JSR) ###############*/
	
	case 0x20: //JSR absolute, 3b 6c
		step = 6;
		PC_backup = PC + 2; //current PC
		low = PC_backup; //casting to char eliminates high byte
		high = (PC_backup >> 8); // short to char cast only retains the high byte after shift
		//saving to stack
		mem.write(SP, high);
		SP = SP - 1; //updating stack
		mem.write(SP, low);
		SP = SP - 1; //fully written to stack
		//fetching instruction to jump to
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high);// new target
		PC = add_ptr; //jumping to PC
		step = step - 1;
		break;

/* ############### Load accumulator with memory (LDA) #############*/

	case 0xA9: // LDA imm, 2b 2c
		step = 2;
		imm = mem.read(PC + 1); // memory
		A = imm; //saving to accumulator 
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xA5: // LDA zp, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1); // zp address of memory value
		imm = mem.read(val_ptr_zp);
		A = imm; //saving to accumulator
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xB5: // LDA zp X, 2b 4c
		step = 4;
		val_ptr_zp = mem.read(PC + 1) + X; // zp address of memory value
		imm = mem.read(val_ptr_zp);
		A = imm; //saving to accumulator
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xAD: //LDA abs, 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); // pointer to operand
		imm = mem.read(val_ptr);
		A = imm; //saving to A
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0xBD: //LDA abs X, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; // pointer to operand
		imm = mem.read(val_ptr);
		A = imm; //saving to A
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;
	
	case 0xB9: //LDA abs Y, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + Y; // pointer to operand
		imm = mem.read(val_ptr);
		A = imm; //saving to A
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	case 0xA1:  // LDA pre indexed X, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X;
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high); //pointer to operand
		imm = mem.read(val_ptr);
		A = imm; //setting A 
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xB1: // LDA post-indexed Y, 2b 5c+
		step = 5;
		add_ptr_zp = mem.read(PC + 1);
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high) + Y;
		imm = mem.read(val_ptr);
		A = imm; //setting accumulator
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	/* ####### Load index X with memory (LDX) #####*/

	case 0xA2: // LDX imm, 2b 2c
		step = 2;
		imm = mem.read(PC + 1); // memory
		X = imm; //saving to X 
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xA6: // LDX zp, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1); // zp address of memory value
		imm = mem.read(val_ptr_zp);
		X = imm; //saving to X
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xB6: // LDX zp Y, 2b 34
		step = 4;
		val_ptr_zp = mem.read(PC + 1) + Y; // zp address of memory value
		imm = mem.read(val_ptr_zp);
		X= imm; //saving to X
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xAE: //LDX abs, 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); // pointer to operand
		imm = mem.read(val_ptr);
		X = imm; //saving to X
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0xBE: //LDX abs Y, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + Y; // pointer to operand
		imm = mem.read(val_ptr);
		X = imm; //saving to X
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	/* ##### Load index Y with memory (LDY) ######*/

	case 0xA0: // LDY imm, 2b 2c
		step = 2;
		imm = mem.read(PC + 1); // memory
		Y = imm; //saving to Y
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xA4: // LDY zp, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1); // zp address of memory value
		imm = mem.read(val_ptr_zp);
		Y = imm; //saving to Y
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xB4: // LDY zp X, 2b 4c
		step = 4;
		val_ptr_zp = mem.read(PC + 1) + X; // zp address of memory value
		imm = mem.read(val_ptr_zp);
		Y = imm; //saving to Y
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xAC: //LDY abs, 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); // pointer to operand
		imm = mem.read(val_ptr);
		Y = imm; //saving to Y
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0xBC: //LDY abs X, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; // pointer to operand
		imm = mem.read(val_ptr);
		Y = imm; //saving to Y
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;

	/* ###### Shift one bit right memory or accumulator (LSR) #####*/

	case 0x4A: // LSR acc, 1b 2c
		step = 2;
		C = (A & 1); //keeping LSB
		A = A >> 1; // right shift
		//setting flags 
		N = 0;
		Z = (A == 0);
		//done
		step = step - 1;
		PC = PC + 1;
		break;

	case 0x46: // LSR zp, 2b 5c;
		step = 5;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp);
		C = (imm & 1); //getting lsb
		imm = imm >> 1; //shifting
		mem.write(val_ptr_zp, imm); //writing back!
		//setting flags 
		N = 0;
		Z = (imm == 0);
		//done
		step = step - 1;
		PC = PC + 2;
		break;

	case 0x56: // LSR zp x, 2b 5c;
		step = 5;
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp);
		C = (imm & 1); //getting lsb
		imm = imm >> 1; //shifting
		mem.write(val_ptr_zp, imm); //writing back!
		//setting flags 
		N = 0;
		Z = (imm == 0);
		//done
		step = step - 1;
		PC = PC + 2;
		break;

	case 0x4E: // LSR absolute, 3b 6c
		step = 5;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); //address of memory element
		imm = mem.read(val_ptr);
		C = (imm & 1);
		imm = imm >> 1; //shift
		mem.write(val_ptr, imm); //writeback
		//setting flags 
		N = 0;
		Z = (imm == 0);
		//done
		step = step - 1;
		PC = PC + 3;
		break;

	case 0x5E: // LSR absolute X, 3b 6c
		step = 5;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; //address of memory element
		imm = mem.read(val_ptr);
		C = (imm & 1);
		imm = imm >> 1; //shift
		mem.write(val_ptr, imm); //writeback
		//setting flags 
		N = 0;
		Z = (imm == 0);
		//done
		step = step - 1;
		PC = PC + 3;
		break;

	/* #### No operation (NOP) #####*/

	case 0xEA: // NOP, 1b 2c
		step = 2;
		//nothing happens !
		PC = PC + 1; 
		step = step - 1;
		break;

	/* OR memory with accumulator (ORA) ####*/

	case 0x09: //ORA immediate, 2b 2c
		step = 2;
		imm = mem.read(PC + 1);
		A = A | imm; //bitwise OR
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x05: // ORA zeropage, 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1); //address of operand in ZP
		imm = mem.read(val_ptr_zp);
		A = A | imm; //bitwise OR
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x15: //ORA Zeropage X, 2b 4c
		step = 4;
		val_ptr_zp = mem.read(PC + 1) + X; //address of operand in ZP
		imm = mem.read(val_ptr_zp);
		A = A | imm; //bitwise OR
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x0D: // ORA abs, 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); //address of operand
		imm = mem.read(val_ptr);
		A = A | imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x1D: // ORA abs X, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; //address of operand
		imm = mem.read(val_ptr);
		A = A | imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;

	case 0x19: // ORA abs Y, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + Y; //address of operand
		imm = mem.read(val_ptr);
		A = A | imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	case 0x01: // ORA pre-indirect X, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X;
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		A = A | imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x11: //ORA post-indirect Y, 2b 5c+
		step = 5;
		add_ptr_zp = mem.read(PC + 1);
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high) + Y;
		imm = mem.read(val_ptr);
		A = A | imm;
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		PC = PC + 2;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;


/* ###### Push accumulator on stack (PHA) ####*/

	case 0x48: //PHA implied, 1b3c
		step = 3;
		mem.write(SP, A);
		SP = SP - 1; //decrementing SP after push
		step = step - 1;
		PC = PC + 1;
		break;

	/* ###### Push SR on stack (PHP) ####*/
	
	case 0x08: // PHP 1b 3c
		step = 3;
		//setting both B flags to 1 as required
		B_l = 1;
		B_h = 1;
		SR_backup = pack_SR(N, V, B_h, B_l, D, I, Z, C);
		//pushing to stack
		mem.write(SP, SR_backup);
		SP = SP - 1; 
		//done
		PC = PC + 1;
		step = step - 1;
		break;

/* ###### Pull A from stack (PLA) ####*/

	case 0x68: //PLA 1b 4c
		step = 4;
		//popping stack
		SP = SP + 1; //adjusting stack top to reflect pop operation (and points to topmost element)
		imm = mem.read(SP);
		A = imm; //loading into A
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		step = step - 1;
		PC = PC + 1;
		break;

/* ### PULL processor status from stack (PLP) #####*/
	
	case 0x28: // PLP 1b 4c
		step = 4;
		//popping stack 
		SP = SP + 1;
		imm = mem.read(SP);
		//setting flags
		C = (imm & 1); //bit 0
		Z = (imm & 0x02); //bit 1;
		I = (imm & 0x04); //bit 2
		D = (imm & 0x08); //bit 3
		B_l = (imm & 0x10); //bit 4
		B_h = (imm & 0x20); //bit 5
		V = (imm & 0x40); //bit 6
		N = (imm & 0x80); //bit 7
		//done getting flags
		PC = PC + 1;
		step = step - 1;
		break;

	/* ####### Rotate one bit left (ROL) ######*/

	case 0x2A: //ROL acc, 1b 2c
		step = 2;
		C = (A & 0x80); //saving MSB of A
		imm = (A << 1) | C; // bit shifting to the left, and inserting carry
		A = imm; //saving to acc
		//setting flags
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 1;
		step = step - 1;
		break;

	case 0x26: //ROL ZP, 2b 5c
		step = 5;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp); //value to rotate
		C = (imm & 0x80);//MSB
		imm = (imm << 1) | C;
		//writing back
		mem.write(val_ptr_zp, imm);
		//setting flags;
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x36: // ROL ZP X, 2b 6c
		step = 6;
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp); //value to rotate
		C = (imm & 0x80);//MSB
		imm = (imm << 1) | C;
		//writing back
		mem.write(val_ptr_zp, imm);
		//setting flags;
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x2E: //ROL absolute, 3b 6c
		step = 6;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); //address of immediate
		imm = mem.read(val_ptr);
		C = (imm & 0x80);
		imm = (imm << 1) | C; //shifting left and inserting carry
		//writing back
		mem.write(val_ptr, imm);
		//setting flags;
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x3E: //ROL absolute X, 3b 7c
		step = 7;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; //address of immediate
		imm = mem.read(val_ptr);
		C = (imm & 0x80);
		imm = (imm << 1) | C; //shifting left and inserting carry
		//writing back
		mem.write(val_ptr, imm);
		//setting flags;
		Z = (imm == 0);
		N = isNegative_8b(imm);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	/* ##########  Rotate one bit right (ROR)  #######*/

	case 0x6A: // ROR A, 1b 2c
		step = 2;
		C = A & 0x01; //fetching LSB to insert later
		A = (A >> 1) | (C << 7); //carry bit is shoved to MSB position, and inserted into MSB of A. computation done!
		//setting flags
		N = isNegative_8b(A);
		Z = (A == 0);
		//done
		PC = PC + 1;
		step = step - 1;
		break;

	case 0x66: // ROR ZP, 2b 5c
		step = 5;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp);
		C = imm & 0x01; //getting LSB to insert
		imm = (imm >> 1) | (C << 7); 
		//writing back
		mem.write(val_ptr_zp, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x76: // ROR ZP x, 2b 6c
		step = 6;
		val_ptr_zp = mem.read(PC + 1) + X; //pointer to operand
		imm = mem.read(val_ptr_zp);
		C = imm & 0x01; //getting LSB to insert
		imm = (imm >> 1) | (C << 7);
		//writing back
		mem.write(val_ptr_zp, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x6E: // ROR absolute, 3b 6c
		step = 6;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high); //pointer to imm
		imm = mem.read(val_ptr);
		C = imm & 0x01; //lsb to insert into msb position later
		imm = (imm >> 1) | (C << 7);
		//writing back
		mem.write(val_ptr, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x7E: // ROR absolute x, 3b 7c
		step = 7;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X; //pointer to imm
		imm = mem.read(val_ptr);
		C = imm & 0x01; //lsb to insert into msb position later
		imm = (imm >> 1) | (C << 7);
		//writing back
		mem.write(val_ptr, imm);
		//setting flags
		N = isNegative_8b(imm);
		Z = (imm == 0);
		//done
		PC = PC + 3;
		step = step - 1;
		break;


	/* ####### Return from interrupt (RTI) ####*/

	case 0x40: // RTI 1b 6c
		 step = 6; 
		 SP = SP + 1; // popping stack
		 SR_backup = mem.read(SP); //reading topmost element (SR)
		 //popping PC
		 SP = SP + 1; 
		 low = mem.read(SP); //low byte of PC (LE!)
		 SP = SP + 1;
		 high = mem.read(SP); //high byte of PC (LE!)
		 PC_backup = concatenate2x8b(low, high); // full address after ISR, to jump to
		 // resetting flags from before ISR
		 C = SR_backup & 0x01;
		 Z = SR_backup & 0x02;
		 I = SR_backup & 0x04;
		 D = SR_backup & 0x08;
		 B_l = SR_backup & 0x10;
		 B_h = SR_backup & 0x20;
		 V = SR_backup & 0x40;
		 N = SR_backup & 0x80;
		 // flags reset, now jumping to correct PC!
		 PC = PC_backup;
		 step = step - 1;
		 break;

/* ####### Return from subroutine (RTS) #####*/

	case 0x60: // RTS 1b 6c
		step = 6;
		//stack ops
		SP = SP + 1;
		low = mem.read(SP);
		SP = SP + 1;
		high = mem.read(SP);
		PC_backup = concatenate2x8b(low, high) + 1; // see JSR instruction. We saved PC + 2 to stack but JSR is 3byte instruction, so this increment is critical so we don't fetch the last byte of JSR instruction
		PC = PC_backup; //saving, ready to jump next time
		//done
		step = step - 1;
		break;

/* ###### Subtract with borrow (SBC) ###*/

	case 0xE9: // SBC imm 2b 2c
		step = 2;
		imm = mem.read(PC + 1);
		//setting carry flag
		Cp = (A >= imm);
		A = A - imm - (~C>> 7); //op on C ensures that if C is initially 0 we are left with 0x01, not 0xFF, given that bool/char addition tends to treat NOT(0) as 255.
		//setting flags
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xE5: // SBC zp 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1);
		imm = mem.read(val_ptr_zp);
		Cp = (A >= imm);
		A = A - imm - (~C >> 7);
		//setting flags
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xF5: // SBC zp x 2b 3c
		step = 3;
		val_ptr_zp = mem.read(PC + 1) + X;
		imm = mem.read(val_ptr_zp);
		Cp = (A >= imm);
		A = A - imm - (~C >> 7);
		//setting flags
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xED: // SBC abs, 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		Cp = (A >= imm);
		A = A - imm - (~C >> 7);
		//setting flags
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0xFD: // SBC abs x, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + X;
		imm = mem.read(val_ptr);
		Cp = (A >= imm);
		A = A - imm - (~C >> 7);
		//setting flags
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, X);
		break;

	case 0xF9: // SBC abs Y, 3b 4c+
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		val_ptr = concatenate2x8b(low, high) + Y;
		imm = mem.read(val_ptr);
		Cp = (A >= imm);
		A = A - imm - (~C >> 7);
		//setting flags
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 3;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	case 0xE1: // SBC preindirect X, 2b 6c;
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X; //pointer to value pointer
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high);
		imm = mem.read(val_ptr);
		Cp = (A >= imm);
		A = A - imm - (~C >> 7);
		//setting flags	
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0xF1: // SBC postindirect y, 2b 5c+;
		step = 5;
		add_ptr_zp = mem.read(PC + 1); //pointer to value pointer
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		val_ptr = concatenate2x8b(low, high) + Y; // address of imm
		imm = mem.read(val_ptr);
		Cp = (A >= imm);
		A = A - imm - (~C >> 7);
		//setting flags
		C = Cp;
		N = isNegative_8b(A);
		Z = (A == 0);
		V = (A > 127); //unsure !
		//done
		PC = PC + 2;
		step = step - 1 + crossing_page_notconcatenated(low, high, Y);
		break;

	/* #### Set carry flag (SEC)###*/

	case 0x38: // SEC 1b 2c
		step = 2;
		C = 1; //setting carry flag
		PC = PC + 1;
		step = step - 1;
		break;

/* #### Set decimal flag (SED)###*/

	case 0xF8: // SED 1b 2c
		step = 2;
		D = 1; //setting decimal flag
		PC = PC + 1;
		step = step - 1;
		break;


/* #### interrupt disable flag (SEI)###*/

	case 0x78: // SEI 1b 2c
		step = 2;
		I = 1; //setting carry flag
		PC = PC + 1;
		step = step - 1;
		break;


/* Store accumulator in memory (STA) ####*/

	case 0x85: // STA zp 2b 3c
		step = 3;
		add_ptr_zp = mem.read(PC + 1);// address to store A contents
		//writing back
		mem.write(add_ptr_zp, A);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x95: // STA ZP X 2b 4c
		step = 4;
		add_ptr_zp = mem.read(PC + 1) + X;// address to store A contents
		//writing back
		mem.write(add_ptr_zp, A);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x8D: // STA abs; 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high); //address to store A contents
		//writing back
		mem.write(add_ptr, A);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x9D: // STA abs x; 3b 5c
		step = 5;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high) + X; //address to store A contents
		//writing back
		mem.write(add_ptr, A);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x99: // STA abs y; 3b 5c
		step = 5;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high) + Y; //address to store A contents
		//writing back
		mem.write(add_ptr, A);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	case 0x81: //STA preindirect x, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1) + X; // pointer to address where A should be stored
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		add_ptr = concatenate2x8b(low, high);// place to store A contents
		//writing back
		mem.write(add_ptr, A);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x91: //SDA postindirect t, 2b 6c
		step = 6;
		add_ptr_zp = mem.read(PC + 1); // pointer to address where A should be stored
		low = mem.read(add_ptr_zp);
		high = mem.read(add_ptr_zp + 1);
		add_ptr = concatenate2x8b(low, high) + Y;// place to store A contents
		//writing back
		mem.write(add_ptr, A);
		//done
		PC = PC + 2;
		step = step - 1;
		break;



/*###### Store index X in memory (STX) #####*/

	case 0x86: // STX ZP, 2b 3c
		step = 3; 
		add_ptr_zp = mem.read(PC + 1);// address to store x contents
		//writing back
		mem.write(add_ptr_zp, X);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x96: // STX ZP Y, 2b 4c
		step = 4;
		add_ptr_zp = mem.read(PC + 1) + Y;// address to store X contents
		//writing back
		mem.write(add_ptr_zp, X);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x8E: // STX abs; 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high); //address to store X contents
		//writing back
		mem.write(add_ptr, X);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

		/*###### Store index Y in memory (STY) #####*/

	case 0x84: // STY ZP, 2b 3c
		step = 3;
		add_ptr_zp = mem.read(PC + 1);// address to store y contents
		//writing back
		mem.write(add_ptr_zp, Y);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x94: // STY ZP X, 2b 4c
		step = 4;
		add_ptr_zp = mem.read(PC + 1) + X; // address to store Y contents
		//writing back
		mem.write(add_ptr_zp, Y);
		//done
		PC = PC + 2;
		step = step - 1;
		break;

	case 0x8C: // STY abs; 3b 4c
		step = 4;
		low = mem.read(PC + 1);
		high = mem.read(PC + 2);
		add_ptr = concatenate2x8b(low, high); //address to store Y contents
		//writing back
		mem.write(add_ptr, Y);
		//done
		PC = PC + 3;
		step = step - 1;
		break;

	/* #### Register Tansfer instructions (TAX, TAY, TSX, TXA, TXS, TYA) ####*/

	case 0xAA: //TAX, 1b 2c
		step = 2;
		X = A; 
		//setting flags
		N = isNegative_8b(X);
		Z = (X == 0);
		PC = PC + 1;
		step = step - 1;
		break;

	case 0xA8: //TAY, 1b 2c
		step = 2;
		Y = A;
		//setting flags
		N = isNegative_8b(Y);
		Z = (Y == 0);
		PC = PC + 1;
		step = step - 1;
		break;

	case 0xBA: //TSX, 1b 2c
		step = 2;
		X = SP;
		//setting flags
		N = isNegative_8b(X);
		Z = (X == 0);
		PC = PC + 1;
		step = step - 1;
		break;

	case 0x8A: //TXA, 1b 2c
		step = 2;
		A = X;
		//setting flags
		N = isNegative_8b(X);
		Z = (X == 0);
		PC = PC + 1;
		step = step - 1;
		break;

	case 0x9A: //TXS, 1b 2c
		step = 2;
		SP = X;
		//no flags set
		PC = PC + 1;
		step = step - 1;
		break;

	case 0x98: //TYA, 1b 2c
		step = 2;
		A = Y;
		//setting flags
		N = isNegative_8b(Y);
		Z = (Y == 0);
		PC = PC + 1;
		step = step - 1;
		break;

	default: // not recognized opcode
		//handle invalid opcode error
		break;
	
}
		
}
