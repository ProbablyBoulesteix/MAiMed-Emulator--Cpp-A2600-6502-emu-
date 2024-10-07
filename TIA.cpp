#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include "TIA.h"

TIA::TIA(std::string colormap_file, int horizontal_res, int vertical_res) {//constructor, create vbuffer
	// loading in colormap
	TIA::load_colormap(colormap_file);
	//initializing vbuffer
	vbuffer = new unsigned char* [vertical_res]; //as with matrices, vertical size goes first -> 1D vector of rows 
	for (int i = 0; i < horizontal_res; i++) { vbuffer[i] = new unsigned char[horizontal_res]; }// filling rows with collums -> 2D ARRAY
	//reseting display counters
	v_counter = 0;
	h_counter = 0;
}

/* void TIA::tia_tick(Processor cpu) { //TIA clock cycle
	
	//get color of pixel to display, part of TIA main loop
	//unsigned char pixel = color_of_pixel(h_counter); // remember, A2600 only sees scanlines
	//int pixel_RGB = get_RGB(pixel); //convert A2600 NTSC color format to RGB for display
	//vbuffer[v_counter][h_counter] = pixel_RGB; //write to display buffer

	//what to do if at end of display
	h_counter = h_counter + 1; //going to next pixel on scanline
	if (!(h_counter < horizontal_res)) { //if we reach display border
		cpu.waiting = 0; //resetting cpu to active state (for previous RDY calls);
		h_counter = 0; //returning to scanline start
		v_counter = v_counter + 1; //next scanline
	}
}*/

void TIA::load_colormap(std::string colormap_file) {
	std::ifstream file(colormap_file);
	std::string code, rgb; //places to store atari 2600 color codes and corresponding rgb, in string form
	int idx;
	int value;
	
	//create colormap array
	colormap = new int[256]; //much larger than needed to account for missin bits, this array will store the color map


	if (!file.is_open()) { std::cerr << "error opening color file" << std::endl; }
	
	else {
		std::string line;
		while (std::getline(file, line)) { 
			//extracting data as str
			std::stringstream ss(line);
			std::getline(ss, code, ',');
			std::getline(ss, rgb);
			//casting hex-formatted str to int for storage
			std::stringstream key;
			std::stringstream val;
			key << std::hex << code;
			val << std::hex << rgb;
			key >> idx;
			val >> value;

			//std::cout << idx << " " << value << std::endl;
			//loading into colormap
			colormap[idx] = value; 
		}
	}
		
}


unsigned int TIA::get_RGB(unsigned char color) {
	// we want color codes such as 0x54 and 0x55 to both return the same match
	color = color & 0xFE; //removing extra bit
	unsigned int hit = colormap[color];
	if (hit == 0xCDCDCDCD) {
		return 0; //return black, color not found
	}
	return hit;
}

unsigned char TIA::check_read(unsigned short address) {
	if ((address >= 0x30) && (address <= 0x3D)) { //address is in correct range for TIA read
		addr_reserved = 1;
		//checking all address
		switch (address) {
		case 0x30:
			return CXM0P;
			break;
		case 0x31:
			return CXM1P;
			break;
		case 0x32:
			return CXP0FB;
			break;
		case 0x33:
			return CXP1FB;
			break;
		case 0x34:
			return CXM0FB;
			break;
		case 0x35:
			return CXM1FB;
			break;
		case 0x36:
			return CXBLPF;
			break;
		case 0x37:
			return CXPPMM;
			break;
		case 0x38:
			return INPT0;
			break;
		case 0x39:
			return INPT1;
		case 0x3A:
			return INPT2;
			break;
		case 0x3B:
			return INPT3;
			break;
		case 0x3C:
			return INPT4;
			break;
		case 0x3D:
			return INPT5;
			break;
		default:
			std::cerr << "Error with TIA read address, address: " << address << std::endl;
			return -1;

		}
	}
	else { //not part of TIA read register space
		addr_reserved = 0; //flag to indicate access is outside of TIA read-only registers
		return 0; //boilerplate return value
	}
	
}
