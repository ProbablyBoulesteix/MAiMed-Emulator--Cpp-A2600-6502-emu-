

#ifndef TIA_H
#define TIA_H

class TIA {
#include "cpu.h"
public:
	//TIA registers Write only
	unsigned char VSYNC; //$00, bit 1. Vertical sync set-clear
	unsigned char VBLANK; //$01, bits 1,6,7, vertical blank set-clear
	unsigned char WSYNC; //$02 (strobe) wait for Hblank, RDY processor
	unsigned char RSYNC; //$03 (strobe), reset hsync counter
	unsigned char NUSIZ0; //$04, bits 0-5, number-size of player-missile 0
	unsigned char NUSIZ1; //$05, bits 0-5, number-size of player-missile 1
	unsigned char COLUP0; //$06, bits 1-7, color-lum of Player and missile 0
	unsigned char COLUP1; //$07, bits 1-7, color-lum of Player and missile 1
	unsigned char COLUPF; //$08, bits 1-7, color-lum of PF and ball
	unsigned char COLUBK; //$09, bits 1-7, color-lum of background
	unsigned char CTRLPF; //$0A, bits 0,1,2,4,5, control PF ball size and colisions
	unsigned char REFP0; //$0B, bit 4, reflect P0
	unsigned char REFP1; //$0C, bit 4, reflect P1
	unsigned char PF0; //$0D, bits 4-7, PF register 0
	unsigned char PF1; //$0E, bits 4-7, PF register 1
	unsigned char PF2; //$0F, bits 4-7, PF register 2 (check documentation on playfield)
	unsigned char RESP0; //$10, strobe, reset player 0
	unsigned char RESP1; //$11, strobe, reset player 1
	unsigned char RESM0; //$12, strobe, reset missile 0
	unsigned char RESM1; //$13, strobe, reset missile 1
	unsigned char RESBL; //$14, strobe, reset ball
	// AUDIO REGISTER TBF, $15-1A
	unsigned char GRP0; // $1B, all bits, graphics player 0
	unsigned char GRP1; // $1C, all bits, graphics player 1
	unsigned char ENAM0; //$1D, bit 1, graphics enable missile 0
	unsigned char ENAM1; //$1E, bit 1, graphics enable missile 1
	unsigned char ENABL; //$1F, bit 1, graphics enable ball
	unsigned char HMP0; //$20, bits 4-7, horizontal motion player 0
	unsigned char HMP1; //$21, bits 4-7, horizontal motion player 1
	unsigned char HMM0; //$22, bits 4-7, horizontal motion missile 0
	unsigned char HMM1; //$23, bits 4-7, horizontal motion missile 1
	unsigned char HMBL; //$24, bits 4-7,, horizontal motion ball
	unsigned char VDELP0; //$25, bit 0, vertical delay player 0
	unsigned char VDELP1; //$26, bit 0, vertical delay player 1
	unsigned char VDELBL; //$27, bit 0, vertical delay ball
	unsigned char RESMP0; //$28, bit 1, reset missile 0 to player 0
	unsigned char RESMP1; //$29, bit1 reset missile 1 to player 1
	unsigned char HMOVE; //$2A strobe, apply horizontal motion
	unsigned char HMCLR; //$2B, strobe, clear horizontal motion registers
	unsigned char CXCLR; //$2C, strobe, clear collision latches
	
	// TIA Read only registers
	unsigned char CXM0P; //$30, bits 7,6, resp. M0-P1 and M0-P0 collision
	unsigned char CXM1P; //$31, bits 7,6, resp. M1-P0 and M1-P1 collision
	unsigned char CXP0FB; //$32, bits 7,6, resp P0-PF, P0-BL collisions
	unsigned char CXP1FB; //$33, bits 7,6, resp P1-PF, P1-BL collisions
	unsigned char CXM0FB; //$34, bits 7,6, resp M0-PF, M0-BL collisions
	unsigned char CXM1FB; //$35, bits 7,6, resp M1-PF, M1-BL collisions
	unsigned char CXBLPF; //$36, bit 7, BL-PF collision
	unsigned char CXPPMM; //$37, bits 7,6, respectively P0-P1, M0-M1 collisions
	unsigned char INPT0; //$38, bit 7, read pot port
	unsigned char INPT1; //$39, bit 7, read pot port
	unsigned char INPT2; //$3A, bit 7, read pot port
	unsigned char INPT3; //$3B, bit 7, read pot port
	unsigned char INPT4; //$3C, bit 7, read input
	unsigned char INPT5; //$3D, bit 7, read input
	
	//video buffer attributes
	unsigned char** vbuffer; //buffer to display
	int horizontal_res; //resolution including blanking sections 
	int vertical_res; // resolution including screen and vsync lines
	//runtime attributes of tia/vbuffer
	bool vsync; //currently in vsync?
	int h_counter; //internal TIA horizontal counter
	int v_counter; //virtual TIA vertical line counter, used to address buffer. DO NOT use as TIA oepration, as it has no such counter in hardware
	
	int* colormap; //array containing the RGB colors corresponding to atari 2600 colors

	TIA(std::string colormap_file, int horizontal_res = 228, int vertical_res = 262); //create empty display buffer and loads in desired colormap, acts as constructor

	unsigned char check_read(unsigned short address); //check to see if read request is part of reserved TIA addresses, may need to return data if so
	void check_write(unsigned short address); //check to see if write request is part of reserved TIA addresses
	void color_of_pixel(unsigned short hori_count); // uses internal registers to determine the color of the pixel that needs to be displayed for the display engine
	void tia_tick(Processor cpu); //cycle the TIA
	
	bool addr_reserved;

	// specific TIA functions
	 void load_colormap(std::string colormap_file);
	 unsigned int get_RGB(unsigned char color_code); //returns pixel color (ASSUMES LOADED COLOR MAP!)



};


#endif // !TIA_H