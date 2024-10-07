#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <string>

#include "memory.h"
#include "cpu.h"


MemIO::MemIO(int ram_size, std::string colormap_file, int horizontal_res, int vertical_res) {
	array_size = ram_size; //number of elements in array	
	//initialising array representing memory
	mem_array = new unsigned char[array_size];
}
unsigned char MemIO::read(unsigned short address) {
	//is this an access to TIA registers?
	unsigned char reg_tia = check_read(address);
	unsigned char value;
	if (is_reserved_TIA == 0) { //address not mapped to tia
		value = mem_array[address];
	}
	else { //address is mapped to tia
		value = reg_tia;
	}
	return value; //returning value to program
}

void MemIO::write(unsigned short address, unsigned char value) {
	//writing to array
	mem_array[address] = value;
}

void MemIO::flush() {
	for (int i = 0; i < array_size; i++) {
		mem_array[i] = 0;
	}
}



void MemIO::load_colormap(std::string colormap_file) {
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


unsigned int MemIO::get_RGB(unsigned char color) {
	// we want color codes such as 0x54 and 0x55 to both return the same match
	color = color & 0xFE; //removing extra bit
	unsigned int hit = colormap[color];
	if (hit == 0xCDCDCDCD) {
		return 0; //return black, color not found
	}
	return hit;
}

unsigned char MemIO::check_read(unsigned short address) {
	if ((address >= 0x30) && (address <= 0x3D)) { //address is in correct range for TIA read
		is_reserved_TIA = 1;
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
		is_reserved_TIA = 0; //flag to indicate access is outside of TIA read-only registers
		return 0; //boilerplate return value
	}

}

void MemIO::check_write(unsigned short address, unsigned char val) {
	if ((address >= 0x00) and (address <= 0x2C)) { //address in range for TIA write/strobe registers
		is_reserved_TIA = 1; //indicates that address is mapped to TIA
		switch (address) {
		case 0x00: //VSYNC reg, bit 1 sets vertical sync
			fct_VSYNC(val);


		}

	}
}

void MemIO::fct_VSYNC(unsigned char val) {
	bool vsync_set = val & 0x02; //get second bits of "val"

	VSYNC = val; //saving to register for future reference

}