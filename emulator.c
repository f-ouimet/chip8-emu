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
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0E0,  0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F

};

typedef struct Chip8 {
  // 4 kb of mem
  uint8_t mem[4096];
  // 16 8 bit regs
  uint8_t Vregs[16];  // args V0 to VF
  uint16_t Ireg;      // 16 bits to store mem addresses
  uint8_t delayTimer; // 8 bits for timers
  uint8_t soundTimer;

  uint16_t PC;            // initialize at 0x200 in constructor, program counter
  uint8_t SP;             // stack pointer
  uint16_t stack[16];     // stack memory for instruct
  uint8_t keypad[16];     // keypad size (16 key hexadecimal)
  uint32_t video[32][64]; // 64 columns (x) and 32 rows(y)

  uint16_t opcode;

} Chip8;

// constructor that allocates memory and returns pointer
struct Chip8 *chip8_new() {
  struct Chip8 *result = (struct Chip8 *)malloc(sizeof(Chip8));
  result->PC = START_ADDRESS;

  for (unsigned int i = 0; i < 80; ++i) {
    result->mem[FONTSET_ADDRESS + i] =
        fontset[i]; // load fontset into memory at address between 0x000 and
                    // 0x1FF
  }

  return result;
}

void chip8_delete(struct Chip8 *chip8) { free(chip8); }

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
