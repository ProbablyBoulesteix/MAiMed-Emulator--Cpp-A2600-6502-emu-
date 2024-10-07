#include <iostream>
#include <opencv2/opencv.hpp>
#include "cpu.h"
#include "memory.h"


int main() {
	
}

/*
int main() {
	Loader load;
	Memory ram(0x10000);
	Processor cpu(ram);


	load.load_from_file("game.a26", ram, 0xF000);
	//printf("%d \n", load.last_size_loaded);
	//unsigned char low = ram.read(0xFFFC);
	//unsigned char high = ram.read(0xFFFD);
	//printf(" high: %x, low: %x \n");


	for (int i = 0; i < 30090; i++) {
		cpu.cpu_tick(ram); 
	}
	

	for (int i = 0; i < 2; i++) {
		printf("%x \n", ram.read(i));
	}
	

	return 0;
}*/