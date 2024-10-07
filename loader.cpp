#include <iostream>
#include <fstream>

#include "loader.h"
#include "memory.h"

int Loader::load_from_file(std::string filename, unsigned short address_start, MemIO mem) {// filename is the directory of the binary file containing the cartridge data, address_start is the first location in memory to load from
	//file is read
	std::ifstream file;
	file.open(filename, std::ios::binary); 

	if (!file.is_open()) { 
		std::cerr << "Error opening file" << std::endl;
		return -1; } //file opening error
	
	//determining size of file
	file.seekg(0, std::ios::end); //placing seek at end of file to get total size
	int fileSize = file.tellg();  //get current seeker position (end) == find file size
	file.seekg(0, std::ios::beg); //restart at file beginning to read

	char* buffer = new char[fileSize]; // creating a buffer to store contents
	file.read(buffer, fileSize); //dumping file contents into buffer, will later be loaded into memory/cartridge space
	int bytesRead = file.gcount(); // total size of file read
	
	file.close(); //closing file 
	

	//checking to see if rom size is coherent with available memory
	int available_space = mem.array_size - address_start; // address prior to start unavailable for load
	if (bytesRead > available_space) {// ROM too large
		std::cerr << "ROM size error ! Not enough space available !" << std::endl;
		return -2;//error
	}

	last_size_loaded = bytesRead;
	//printf("%d", last_size_loaded);
	//loading into memory
	for (int i = 0; i < (bytesRead); i++) {//iterating across the entire file
		mem.write((i + address_start), buffer[i]); //loading bytes into RAM one by one, from starting address
	}
	 //keeping track of loaded contents 
	delete[] buffer; //deleting loading buffer
	return 0;

}