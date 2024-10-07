

#pragma once
#define LOADER_H

#include "memory.h"


class Loader {
public:
	int last_size_loaded; //keeps track of the size of the last file that was loaded
	int load_from_file(std::string filename, unsigned short address_start, MemIO mem);

};




