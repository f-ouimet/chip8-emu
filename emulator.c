#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define START_ADDRESS 0x200
#define FONTSET_ADDRESS 0x50
uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4

};

typedef struct Chip8 {
  // 4 kb of mem
  uint8_t mem[4096];
  // 16 8 bit regs
  uint8_t Vregs[16];
  uint16_t Ireg;
  uint8_t delayTimer;
  uint8_t soundTimer;

  uint16_t PC; // initialize at 0x200 in constructor
  uint8_t SP;
  uint16_t stack[16];
  uint8_t keypad[16];
  uint32_t video[32][64];

} Chip8;

// constructor that allocates memory and returns pointer
struct Chip8 *chip8_new() {
  struct Chip8 *result = (struct Chip8 *)malloc(sizeof(Chip8));
  result->PC = START_ADDRESS;

  return result;
}

void loadROM(Chip8 *chip8, const char *filepath) {
  FILE *file = fopen(filepath, "rb");
  if (file == NULL) {
    perror("Failed to open file");
    return;
  }
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(size);
  if (buffer == NULL) {
    perror("mem alloc failed");
    fclose(file);
    return;
  }
  fread(buffer, 1, size, file);
  fclose(file);

  for (long i = 0; i < size; ++i) {
    chip8->mem[START_ADDRESS + i] = buffer[i];
  }
  free(buffer);
}
