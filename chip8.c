#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "chip8.h"

uint16_t opcode = 0;
uint8_t memory[0x10000];
uint8_t SV[8];
uint8_t V[16];
uint16_t I = 0;
uint16_t pc = 0;
int16_t delay_timer = 0;
int16_t sound_timer = 0;
uint16_t stack[16];
uint8_t sp = 0;
uint8_t keys[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
bool drawFlag = false;

bool paused = false;
bool playing = false;
bool extendedScreen = 0;

bool xochip = false;
uint8_t pattern[16];
uint8_t plane = 1;

uint8_t game_data[3584];
uint8_t canvas_data[3][8192];
uint8_t controlMap[16];

unsigned char fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};
uint16_t  fontset_ten[80] = {
	0xC67C, 0xDECE, 0xF6D6, 0xC6E6, 0x007C, // 0
	0x3010, 0x30F0, 0x3030, 0x3030, 0x00FC, // 1
	0xCC78, 0x0CCC, 0x3018, 0xCC60, 0x00FC, // 2
	0xCC78, 0x0C0C, 0x0C38, 0xCC0C, 0x0078, // 3
	0x1C0C, 0x6C3C, 0xFECC, 0x0C0C, 0x001E, // 4
	0xC0FC, 0xC0C0, 0x0CF8, 0xCC0C, 0x0078, // 5
	0x6038, 0xC0C0, 0xCCF8, 0xCCCC, 0x0078, // 6
	0xC6FE, 0x06C6, 0x180C, 0x3030, 0x0030, // 7
	0xCC78, 0xECCC, 0xDC78, 0xCCCC, 0x0078, // 8
	0xC67C, 0xC6C6, 0x0C7E, 0x3018, 0x0070, // 9
	0x7830, 0xCCCC, 0xFCCC, 0xCCCC, 0x00CC, // A
	0x66FC, 0x6666, 0x667C, 0x6666, 0x00FC, // B
	0x663C, 0xC0C6, 0xC0C0, 0x66C6, 0x003C, // C
	0x6CF8, 0x6666, 0x6666, 0x6C66, 0x00F8, // D
	0x62FE, 0x6460, 0x647C, 0x6260, 0x00FE, // E
	0x66FE, 0x6462, 0x647C, 0x6060, 0x00F0  // F
};

uint8_t step;
uint16_t pixel;

uint8_t _y;
uint8_t _x;

uint8_t screen_width;
uint8_t screen_height;
uint16_t pixel_number;

void initialize() {
	
	opcode = I = sp = delay_timer = sound_timer = 0;
	pc = 0x200;
	
	extendedScreen = 0;
	screen_width = 64;
	screen_height = 32;
	
	pixel_number = 2048;
	
	plane = 1;

	memset(canvas_data[0], 0, 8192);
	memset(canvas_data[1], 0, 8192);
		
	memset(keys, 0, 16);
	memset(stack, 0, 16);
	memset(V, 0, 16);
	memset(SV, 0, 8);
	memset(memory, 0, 0x10000);
	memset(pattern, 0, 16);
	
	memcpy(memory, fontset, 80);
	memcpy(memory + 80, fontset_ten, 80);
	
	srand(time(NULL));
	setbuf(stdout, NULL);
}

bool loadProgram(char *fileName) {
	int i;
	uint16_t romSize;
	FILE *fileptr;
	char *buffer;
	
	if(access(fileName, F_OK) != -1) {
		playing = true;
		paused = false;
		
		fileptr = fopen(fileName, "rb");
		fseek(fileptr, 0, SEEK_END);
		romSize = ftell(fileptr);
		rewind(fileptr);
		
		buffer = (char*)malloc((romSize+1) * sizeof(char));
		fread(buffer, romSize, 1, fileptr);
		fclose(fileptr);
		
		initialize();
		
		if((4096-512) > romSize) {
			for(i = 0; i < romSize; ++i) {
				memory[i + 512] = (uint8_t)buffer[i];
			}
		}
		return 1;
	} else {
		return 0;
	}
}

void skip() {
	pc += (opcode == 0xf000) ? 4 : 2;
}

void emulateCycle(uint8_t steps) {
		
	for(step = 0; step < steps; ++step) {
		int i;
		uint8_t x;
		uint8_t y;
		opcode = (memory[pc] << 8) | memory[pc+1];
		x = (opcode & 0x0f00) >> 8;
		y = (opcode & 0x00f0) >> 4;
		
		pc += 2;
		
		switch(opcode & 0xf000) {
			case 0x0000: {
				switch(opcode & 0x00f0) {
					case 0x00c0: { //SCD
						uint8_t n = (opcode & 0x000f);
						uint8_t *disp = &canvas_data[0][0];
						uint8_t j;
						
						drawFlag = true; 
						
						for(j = 0; j < 2; j++) {
							if(plane & (j + 1) == 0) continue;
							disp = &canvas_data[j][0];
							for(i = screen_height-2; i >= 0; i--) {
								memcpy(disp + (i+n)*screen_width, disp + i*screen_width, screen_width);
								memset(disp + i*screen_width, 0, screen_width);
							}
						}
						
						break;
					}
					case 0x00d0: { //SCU
						uint8_t n = (opcode & 0x000f);
						uint8_t *disp = &canvas_data[0][0];
						uint8_t j;
						
						drawFlag = true; 
						
						for(j = 0; j < 2; j++) {
							if(plane & (j + 1) == 0) continue;
							disp = &canvas_data[j][0];
							for(i = 0; i < screen_height-2; i--) {
								memcpy(disp + i*screen_width, disp + (i+n)*screen_width, screen_width);
								memset(disp + (i+n)*screen_width, 0, screen_width);
							}
						}
						
						break;
					}
					break;
				}
				switch(opcode & 0x00ff) {
					case 0x00e0:
						drawFlag = true;
						memset(canvas_data[0], 0, pixel_number);
						if(plane & 2 == 0) continue;
						memset(canvas_data[1], 0, pixel_number);
						break;
					case 0x00ee:
						pc = stack[(--sp)&0xf];
						break;
					case 0x00fb: { //SCR
						uint8_t *disp = &canvas_data[0][0];
						uint8_t j;
						
						for(j = 0; j < 2; j++) {
							if(plane & (j + 1) == 0) continue;
							disp = &canvas_data[j][0];
							for(i = 0; i < screen_height; i++) {
								memmove(disp + 4, disp, screen_width - 4);
								memset(disp, 0, 4);
								disp += screen_width;
							}
						}
						break;
					}
					case 0x00fc: { //SCL
						uint8_t *disp = &canvas_data[0][0];
						uint8_t j;
						
						for(j = 0; j < 2; j++) {
							if(plane & (j + 1) == 0) continue;
							disp = &canvas_data[j][0];
							for(i = 0; i < screen_height; i++) {
								memmove(disp, disp + 4, screen_width - 4);
								memset(disp + screen_width - 4, 0, 4);
								disp += screen_width;
							}
						}
						break;
					}
					case 0x00fd:
						playing = 0;
						//exit
						break;
					case 0x00fe:
						extendedScreen = 0;
						screen_width = 64;
						screen_height = 32;
						pixel_number = 2048;
						break;
					case 0x00ff:
						extendedScreen = 1;
						screen_width = 128;
						screen_height = 64;
						pixel_number = 8192;
						break;
					default:
						pc = (pc & 0x0fff);
						break;
				}
				break;
			}
			case 0x1000: {
				pc = (opcode & 0x0fff);
				break;
			}
			case 0x2000: {
				stack[sp++] = pc;
				pc = (opcode & 0x0fff);
				break;
			}
			case 0x3000: {
				if(V[x] == (opcode & 0x00ff))
					skip();
				break;
			}
			case 0x4000: {
				if(V[x] != (opcode & 0x00ff))
					skip();
				break;
			}
			case 0x5000: {
				switch(opcode & 0x000f) {
					case 0x0000: {
						if(V[x] == V[y])
							skip();
						break;
					}
					case 0x0002: {
						uint8_t dist = abs(x - y);
						uint8_t z = 0;
						if(x < y) {
							for(z = 0; z <= dist; z++)
								memory[I + z] = V[x + z];
						} else {
							for(z = 0; z <= dist; z++)
								memory[I + z] = V[x - z];
						}
						break;
					}
					case 0x0003: {
						uint8_t dist = abs(x - y);
						uint8_t z = 0;
						if(x < y) {
							for(z = 0; z <= dist; z++)
								memory[x + z] = V[I + z];
						} else {
							for(z = 0; z <= dist; z++)
								memory[x - z] = V[I + z];
						}
						break;
					}
					break;
				}
				break;
			}
			case 0x6000: {
				V[x] = (opcode & 0x00ff);
				break;
			}
			case 0x7000: {
				V[x] = (V[x] + (opcode & 0x00ff)) & 0xff;
				break;
			}
			case 0x8000: {
				switch(opcode & 0x000f) {
					case 0x0000: {
						V[x]  = V[y];
						break;
					}
					case 0x0001: {
						V[x] |= V[y];
						break;
					}
					case 0x0002: {
						V[x] &= V[y];
						break;
					}
					case 0x0003: {
						V[x] ^= V[y];
						break;
					}
					case 0x0004: {
						V[0xf] = (V[x] + V[y] > 0xff);
						V[x] += V[y];
						V[x] &= 255;
						break;
					}
					case 0x0005: {
						V[0xf] = V[x] >= V[y];
						V[x] -= V[y];
						break;
					}
					case 0x0006: {
						V[0xf] = V[x] & 1;
						V[x] >>= 1;
						break;
					}
					case 0x0007: {
						V[0xf] = V[y] >= V[x];
						V[x] = V[y] - V[x];
						break;
					}
					case 0x000E: {
						V[0xf] = V[x] >> 7;
						V[x] <<= 1;
						break;
					}
					break;
				}
				break;
			}
			case 0x9000: {
				if(V[x] != V[y])
					skip();
				break;
			}
			case 0xa000: {
				I = (opcode & 0x0fff);
				break;
			}
			case 0xb000: {
				pc = V[0] + (opcode & 0x0fff);
				break;
			}
			case 0xc000: {
				V[x] = (rand() & 0xff) & (opcode & 0x00FF);
				break;
			}
			case 0xd000: {
				uint8_t xd = V[x];
				uint8_t yd = V[y];
				uint8_t height = (opcode & 0x000f);
				uint8_t layer;
				uint16_t index;
				
				V[0xf] = 0;
				
				drawFlag = true;
				
				for(layer = 0; layer < 2; layer++) {
					if(plane & (layer + 1) == 0) continue;
					if(extendedScreen) {
						//Extended screen DXY0
						uint8_t cols = 1;
						if(height == 0) {
							cols = 2;
							height = 16;
						}
						for(_y = 0; _y < height; ++_y) {
							pixel = memory[I + (cols*_y)];
							if(cols == 2) {
								pixel <<= 8;
								pixel |= memory[I + (_y << 1)+1];
							}
							for(_x = 0; _x < (cols << 3); ++_x) {
								if((pixel & (((cols == 2) ? 0x8000 : 0x80) >> _x)) != 0) {
									index = (((xd + _x) & 0x7f) + (((yd + _y) & 0x3f) << 7));
									V[0xf] |= canvas_data[layer][index] & 1;
									if (canvas_data[layer][index])
										canvas_data[layer][index] = 0;
									else
										canvas_data[layer][index] = 1;
								}
							}
						}
					} else {
						//Normal screen DXYN
						if(height == 0) height = 16;
						for(_y = 0; _y < height; ++_y) {
							pixel = memory[I + _y];
							for(_x = 0; _x < 8; ++_x) {
								if((pixel & (0x80 >> _x)) != 0) {
									index = (((xd + _x) & 0x3f) + (((yd + _y) & 0x1f) << 6));
									V[0xf] |= canvas_data[layer][index] & 1;
									if (canvas_data[layer][index])
										canvas_data[layer][index] = 0;
									else
										canvas_data[layer][index] = 1;
								}
							}
						}
					}
				}
				
				break;
			}
			case 0xe000: {
				switch(opcode & 0x00ff) {
					case 0x009e: {
						if(keys[V[x]])
							skip();
						break;
					}
					case 0x00a1: {
						if(!keys[V[x]])
							skip();
						break;
					}
				}
				break;
			}
			case 0xf000: {
				switch(opcode & 0x00ff) {
					case 0x0000: {
						I = opcode & 0xffff;
						pc += 2;
						break;
					}
					case 0x0001: {
						plane = (x & 0x3);
						break;
					}
					case 0x0002: {
						uint8_t z;
						for(z = 0; z < 16; z++) {
							pattern[z] = memory[I + z];
						}
						break;
					}
					case 0x0007: {
						V[x] = delay_timer;
						break;
					}
					case 0x000A: {
						bool key_pressed = false;
						pc -= 2;
						
						for(i = 0; i < 16; ++i) {
							if(keys[i]) {
								V[x] = i;
								pc += 2;
								key_pressed = true;
								break;
							}
						}
						
						if(!key_pressed)
							return;
					}
					case 0x0015: {
						delay_timer = V[x];
						break;
					}
					case 0x0018: {
						sound_timer = V[x];
						break;
					}
					case 0x001E: {
						V[0xf] = (I + V[x] > 0xff);
						I = (I + V[x]) & 0xffff;
						break;
					}
					case 0x0029: {
						I = (V[x] & 0xf) * 5;
						break;
					}
					case 0x0030: {
						I = (V[x] & 0xf) * 10 + 80;
						break;
					}
					case 0x0033: {
						memory[ I ] = V[x] / 100;
						memory[I+1] = (V[x] / 10) % 10;
						memory[I+2] = V[x] % 10;
						break;
					}
					case 0x0055: {
						for(i = 0; i <= x; ++i) {
							memory[I + i] = V[i];
						}
						break;
					}
					case 0x0065: {
						for(i = 0; i <= x; ++i) {
							V[i] = memory[I + i];
						}
						break;
					}
					case 0x0075: {
						if (x > 7) x = 7;
						for(i = 0; i <= x; ++i) {
							SV[i] = V[i];
						}
						break;
					}
					case 0x0085: {
						if (x > 7) x = 7;
						for(i = 0; i <= x; ++i) {
							V[i] = SV[i];
						}
						break;
					}
				}
				break;
			}
			default:
				break;
		}
	}
	if(sound_timer > 0) {
		--sound_timer;
	}
	if(delay_timer > 0) {
		--delay_timer;
	}
}















