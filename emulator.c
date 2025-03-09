/**
 *  @author : Felix Ouimet
 *  https://github.com/f-ouimet/chip8-emu
 *  Interpreter(emulator) for a chip-8 programs.
 *
 *Links used for code source / inspo:
 *            http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
 *            https://tobiasvl.github.io/blog/write-a-chip-8-emulator/
 *            https://austinmorlan.com/posts/chip8_emulator/
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  char* OS = "Windows";
#elif __linux__
  char* OS = "Linux";
#else
  char* OS = "other";
#endif
#define START_ADDRESS 0x200  // offset for ram
#define FONTSET_ADDRESS 0x50 // reserved mem space

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
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F

};

/**
 * struct to emulate a Chip8 computer.
 */
typedef struct Chip8 {
  // 4 kb of mem
  uint8_t mem[4096];
  // 16 8 bit regs
  uint8_t Vregs[16];  // args V0 to VF
  uint16_t Ireg;      // 16 bits to store mem addresses
  uint8_t delayTimer; // 8 bits regs for timers
  uint8_t soundTimer;

  uint16_t PC; // initialize at 0x200 in constructor, program counter

  // The stack is an array of 16 16-bit values, used to store the address that
  // the interpreter
  //...should return to when finished with a subroutine.
  // Chip-8 allows for up to 16 levels of nested subroutines.
  uint8_t SP;         // stack pointer
  uint16_t stack[16]; // stack memory for instruct

  uint8_t keypad[16]; // keypad size (16 key hexadecimal)
  // Array for display, 2d grid flattened in 1d
  uint32_t video[64 * 32]; // 64 columns (x) and 32 rows(y)

  uint16_t opcode;

} Chip8;

/**
 * Constructor that allocates memory and returns pointer.
 * @return
 */
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
// Clears allocated memory
void chip8_delete(struct Chip8 *chip8) { free(chip8); }

/**Loads rom file in a buffer that is then placed in virtual RAM at the
 *START_ADDRESS
 *
 *@param chip8
 *@param filepath
 */
void loadROM(Chip8 *chip8, const char *filepath) {
  FILE *file = fopen(filepath, "rb");
  if (file == NULL) {
    perror("Failed to open file");
    return;
  }
  fseek(file, 0, SEEK_END);
  const long size = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(size);
  if (buffer == NULL) {
    perror("Memory alloc failed");
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

// Instruction 00E0
void clear_screen(struct Chip8 *chip8) {
  memset(chip8->video, 0, sizeof(chip8->video));
}

// Instruction 1NNN
void jump(struct Chip8 *chip8, uint16_t opcode) {
  const uint16_t address = opcode & 0x0FFF; // we keep last 3 hex digits
  chip8->PC = address;
}

// Instruction 6XNN/6XKK
void LD(struct Chip8 *chip8, uint16_t opcode) {
  const uint8_t kk_value = opcode & 0xFF;
  const uint8_t x = (opcode >> 8) & 0xF;
  chip8->Vregs[x] = kk_value;
}
// INSTRUCTION 7XNN
void ADD(struct Chip8 *chip8, uint16_t opcode) {
  const uint8_t nn_value = opcode & 0xFF;
  const uint8_t x = (opcode >> 8) & 0xF;
  chip8->Vregs[x] += nn_value;
}
// INSTRUCTION ANNN
void LD_I(struct Chip8 *chip8, uint16_t opcode) {
  chip8->Ireg = opcode & 0x0FFF;
}
// INSTRUCTION DXYN
void DRAW(struct Chip8 *chip8, uint16_t opcode) {
  const uint8_t Vx = (opcode & 0x0F00) >> 8;
  const uint8_t Vy = (opcode & 0x00F0) >> 4;

  const uint8_t x = chip8->Vregs[Vx] % 64; // Wrap around if x > 63
  const uint8_t y = chip8->Vregs[Vy] % 32; // Wrap around if y > 31

  const uint8_t n = opcode & 0x000F;
  chip8->Vregs[0xF] = 0;

  for (int i = 0; i < n; i++) {
    const uint8_t data = chip8->mem[chip8->Ireg + i];
    for (int j = 0; j < 8; j++) {
      if (data & (0x80 >> j)) {
        const int screenX = (x + j) % 64;
        const int screenY = (y + i) % 32;

        const int pixelIndex = screenY * 64 + screenX;
        if (chip8->video[pixelIndex] == 1) {
          // If the pixel was already set, a collision occurred
          chip8->Vregs[0xF] = 1;
        }
        chip8->video[pixelIndex] ^= 1;
      }
    }
  }

  // chip8->Vregs[15] = 1; if collision
}

void clear_console() {
  if (strcmp(OS, "Windows") == 0) {system("cls");}
  else{system("clear");}
}

void draw_console(const struct Chip8 *chip8) { //TODO: make more fluid by printing line instead of char
  clear_console();
  for (unsigned int i = 0; i < 64 * 32; ++i) {
    // Print a block character for set pixels, otherwise a space
    if (chip8->video[i] == 1) {
      if (strcmp(OS, "Windows") == 0) {printf("#");}
      else{printf("█");}
       // other char options: █ ■ ▮ ▓
    } else {
      printf(" ");
    }

    // Print a newline after every 64 pixels (end of a row)
    if ((i + 1) % 64 == 0) {
      printf("\n");
    }
  }
}


/**
 *
 * @param chip8 our Chip8 instance
 * @param opcode current operation code
 */
//TODO: TEST ALL INSTRUCTIONS
void exec_instruction(struct Chip8 *chip8, const uint16_t opcode) {
  switch (opcode >> 12 & 0xF) {
  case 0x0: {
    if (opcode == 0x00E0) {
      clear_screen(chip8);
    } else if (opcode == 0x00EE) {
      // RET
      // The interpreter sets the program counter to the address at the top of
      // the stack, then subtracts 1 from the stack pointer.
      chip8->SP--;
      chip8->PC = chip8->stack[chip8->SP];
    } else {
      // SYS addr, ignored by modern computer
    }
    break;
  }
  case 0x1: {
    // 1NNN
    jump(chip8, opcode);
    break;
  }
  case 0x2: {
    //CALL
    chip8->SP++;
    chip8->stack[chip8->SP] = chip8->PC;
    chip8->PC = opcode & 0x0FFF;
    break;
  }
    //SE Vx byte]
  case 0x3: {
    uint8_t x = opcode & 0x0F00;
    x = x >> 8;
    const uint8_t y = opcode & 0x00FF;
    if (chip8->Vregs[x] == y) {
      chip8->PC=+2;
    }
    break;
  }
    //SNE Vx byte
  case 0x4: {
    uint8_t x = opcode & 0x0F00;
    x = x >> 8;
    const uint8_t y = opcode & 0x00FF;
    if (chip8->Vregs[x] == y) {
      chip8->PC=+2;
    }
    break;
  }
    //SE Vx Vy
  case 0x5: {
    uint8_t x = opcode & 0x0F00;
    x = x >> 8;
    uint8_t y = opcode >> 0x00F0;
    y = y >> 4;
    if (chip8->Vregs[x] == chip8->Vregs[y]) {
      chip8->PC=+2;
    }
    break;
  }
    //LD value in reg
  case 0x6: {
    LD(chip8, opcode);
    break;
  }
  case 0x7: {
    ADD(chip8, opcode);
    break;
  }
  case 0x8: {
    //Vregs[0xF] is used as carry bit for the operations here
    const uint8_t sub_op = opcode & 0x000F;
    uint8_t x = opcode & 0x0F00 >> 8;
    uint8_t y = opcode & 0x00F0 >> 4;
    switch (sub_op) {
      //LD value of reg y in reg x
      case 0x0: {
        chip8->Vregs[x] = chip8->Vregs[y];
        break;
      }
      //OR
      case 0x1: {
        chip8->Vregs[x] |= chip8->Vregs[y];
        break;
      }
      //AND
      case 0x2: {
        chip8->Vregs[x] &= chip8->Vregs[y];
        break;
      }
      //XOR
      case 0x3: {
        chip8->Vregs[x] ^= chip8->Vregs[y];
        break;
      }
      //ADD
      case 0x4: {
        const uint16_t total = chip8->Vregs[x] + chip8->Vregs[y];
        if (total > 0xFF)
          chip8->Vregs[0xF] = 1;
        else
          chip8->Vregs[0xF] = 0;
        chip8->Vregs[x] = total & 0xFF;
      }
      //SUB
      case 0x5:{
        if (chip8->Vregs[x] > chip8->Vregs[y])
          chip8->Vregs[0xF] = 1;
        else
          chip8->Vregs[0xF] = 0;
        chip8->Vregs[x] = chip8->Vregs[x] - chip8->Vregs[y];
        break;
      }
      //SHR
      case 0x6: {
        if (chip8->Vregs[x] % 2 == 1)
          chip8->Vregs[0xF] = 1;
        else
          chip8->Vregs[0xF] = 0;
        chip8->Vregs[x] = chip8->Vregs[x] / 2;
      }
      //SUBN
      case 0x7: {
        if (chip8->Vregs[y] > chip8->Vregs[x])
          chip8->Vregs[0xF] = 1;
        else
          chip8->Vregs[0xF] = 0;
        chip8->Vregs[x] = chip8->Vregs[y]-chip8->Vregs[x];
      }
      //SHL
      case 0xE: {
        if ((chip8->Vregs[x] & 0x80) == 0x80)
          chip8->Vregs[0xF] = 1;
        else
          chip8->Vregs[0xF] = 0;
        chip8->Vregs[x] = chip8->Vregs[x] * 2;
        break;
      }
      default: {
        printf("Invalid opcode");
        printf("opcode = %04x\n", opcode);
        exit(EXIT_FAILURE);
      }
    }
    break;
  }
  case 0x9: {
    uint8_t x = opcode & 0x0F00;
    x = x >> 8;
    uint8_t y = opcode >> 0x00F0;
    y = y >> 4;
    if (chip8->Vregs[x] != chip8->Vregs[y]) {
      chip8->PC=+2;
    }
    break;
  }
  case 0xA: {
    LD_I(chip8, opcode);
    break;
  }
  case 0xB: {
    break;
  }
  case 0xC: {
    break;
  }
  case 0xD: {
    DRAW(chip8, opcode);
    break;
  }
  case 0xE: {
    break;
  }
  case 0xF: {
    break;
  }
  default: {
    printf("Invalid opcode");
    printf("opcode = %04x\n", opcode);
    exit(EXIT_FAILURE);
  }
  }
}

/**
 *
 * @param argc arg count
 * @param argv should be <program> , <ROM file path>
 * @return 1 if exit_failure, 0 if exit_success
 */
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <ROM file>\n", argv[0]);
    exit(1);
  }
  struct Chip8 *chip8 = chip8_new();

  printf("Loading ROM: %s\n", argv[1]);
  loadROM(chip8, argv[1]);
  /*Clear the terminal screen for program initialization
   *Alternatively
   *#define clear() printf("\033[H\033[J")
   */
  clear_console();


  // Main cpu loop below
  while (1) {
    // Fetch opcode (16 bits) using program counter
    const uint16_t opcode =
        chip8->mem[chip8->PC] << 8 | chip8->mem[chip8->PC + 1];
    chip8->PC += 2;
    // Test print
    // printf("opcode: %04x\n", opcode);
    // Exec the code
    exec_instruction(chip8, opcode);
    draw_console(chip8);
    //sound impl:
    //printf("\a");
    //alternatives:
    //windows.h Beep
    //Linux:
    // int fd = open("/dev/tty", O_WRONLY);
    // if (fd != -1) {
    //   write(fd, "\a", 1);
    //   close(fd);
    // }
  }
  //if exit command is implemented: exit(0);
}
