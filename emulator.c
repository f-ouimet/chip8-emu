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
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <Windows.h>
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
typedef struct chip_8_ {
  // 4 kb of mem
  uint8_t mem[4096];
  // 16 8 bit regs
  uint8_t Vregs[16];  // args V0 to VF
  uint16_t Ireg;      // 16 bits to store mem addresses
  uint8_t delay_timer; // 8 bits regs for timers
  uint8_t sound_timer;

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
  uint8_t keyboard[16];

} Chip8;

/**
 * Constructor that allocates memory and returns pointer.
 * @return
 */
struct chip_8_ *chip8_new() {
  struct chip_8_ *result = (struct chip_8_ *)malloc(sizeof(Chip8));
  result->PC = START_ADDRESS;

  for (unsigned int i = 0; i < 80; ++i) {
    result->mem[FONTSET_ADDRESS + i] =
        fontset[i]; // load fontset into memory at address between 0x000 and
                    // 0x1FF
  }

  return result;
}
// Clears allocated memory
void chip8_delete(struct chip_8_ *chip8) { free(chip8); }

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
void clear_screen(struct chip_8_ *chip8) {
  memset(chip8->video, 0, sizeof(chip8->video));
}

// Instruction 1NNN
void jump(struct chip_8_ *chip8, uint16_t opcode) {
  const uint16_t address = opcode & 0x0FFFu; // we keep last 3 hex digits
  chip8->PC = address;
}

// Instruction 6XNN/6XKK
void LD(struct chip_8_ *chip8, uint16_t opcode) {
  const uint8_t kk_value = opcode & 0xFFu;
  const uint8_t x = (opcode >> 8u) & 0xFu;
  chip8->Vregs[x] = kk_value;
}
// INSTRUCTION 7XNN
void ADD(struct chip_8_ *chip8, uint16_t opcode) {
  const uint8_t nn_value = opcode & 0xFFu;
  const uint8_t x = (opcode >> 8u) & 0xFu;
  chip8->Vregs[x] += nn_value;
}
// INSTRUCTION ANNN
void LD_I(struct chip_8_ *chip8, uint16_t opcode) {
  chip8->Ireg = opcode & 0x0FFF;
}
// INSTRUCTION DXYN
void DRAW(struct chip_8_ *chip8, uint16_t opcode) {
  const uint8_t Vx = (opcode & 0x0F00u) >> 8u;
  const uint8_t Vy = (opcode & 0x00F0u) >> 4u;

  const uint8_t x = chip8->Vregs[Vx] % 64; // Wrap around if x > 63
  const uint8_t y = chip8->Vregs[Vy] % 32; // Wrap around if y > 31

  const uint8_t n = opcode & 0x000Fu;
  chip8->Vregs[0xFu] = 0;

  for (int i = 0; i < n; i++) {
    const uint8_t data = chip8->mem[chip8->Ireg + i];
    for (int j = 0; j < 8; j++) {
      if (data & (0x80u >> j)) {
        const int screenX = (x + j) % 64;
        const int screenY = (y + i) % 32;

        const int pixelIndex = screenY * 64 + screenX;
        if (chip8->video[pixelIndex] == 1) {
          // If the pixel was already set, a collision occurred
          chip8->Vregs[0xFu] = 1;
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

void draw_console(const struct chip_8_ *chip8) { //TODO: make more fluid by printing line instead of char
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
 * translates a keypress on keyboard to a chip8 value keypress
 * keys mapped as: rows are 1, q, a, z
 *                columns are 1, 2, 3, 4
 * chip8 keyboard:
 * 1 2 3 C
 * 4 5 6 D
 * 7 8 9 E
 * A 0 B F
 * @param key the key pressed on our actual keyboard
 * @return value of key in CHIP-8 hex keyboard
 */
uint8_t chip8_keypress(struct chip_8_ *chip8, const int key) {
  chip8->keypad[key] = 0;
  switch (key) {
    case'1':
      return 0x1;
    case'2':
      return 0x2;
    case'3':
      return 0x3;
    case'4':
      return 0xC;
    case'q':
      return 0x4;
    case'w':
      return 0x5;
    case'e':
      return 0x6;
    case'r':
      return 0xD;
    case'a':
      return 0x7;
    case's':
      return 0x8;
    case'd':
      return 0x9;
    case'f':
      return 0xE;
    case'z':
      return 0xA;
    case'x':
      return 0x0;
    case'c':
      return 0xB;
    case'v':
      return 0xF;
    default:{
      chip8->PC -=2;//need to execute instruction again
      return 0xFF;//junk value
    }
  }

}

/**
 *
 * @param chip8 our Chip8 instance
 * @param opcode current operation code
 */
//TODO: TEST ALL INSTRUCTIONS
void exec_instruction(struct chip_8_ *chip8, const uint16_t opcode) {
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
    chip8->stack[chip8->SP] = chip8->PC;
    ++chip8->SP;
    chip8->PC = opcode & 0x0FFFu;
    break;
  }
    //SE Vx byte
  case 0x3: {
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    const uint8_t y = opcode & 0x00FFu;
    if (chip8->Vregs[x] == y) {
      chip8->PC+=2;
    }
    break;
  }
    //SNE Vx byte
  case 0x4: {
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    const uint8_t y = opcode & 0x00FFu;
    if (chip8->Vregs[x] != y) {
      chip8->PC+=2;
    }
    break;
  }
    //SE Vx Vy
  case 0x5: {
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    const uint8_t y = (opcode & 0x00F0u) >> 4u;

    if (chip8->Vregs[x] == chip8->Vregs[y]) {
      chip8->PC+=2;
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
    //V regs[0xF] is used as carry bit for the operations here
    const uint8_t sub_op = opcode & 0x000Fu;
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    const uint8_t y = (opcode & 0x00F0u) >> 4u;
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
        chip8->Vregs[x] = total & 0xFFu;
        if (total > 0xFFu)
          chip8->Vregs[0xFu] = 1u;
        else
          chip8->Vregs[0xFu] = 0;
        break;
      }
      //SUB
      case 0x5:{
        chip8->Vregs[x] = chip8->Vregs[x] - chip8->Vregs[y];
        if (chip8->Vregs[x] > chip8->Vregs[y])
          chip8->Vregs[0xFu] = 1u;
        else
          chip8->Vregs[0xFu] = 0u;
        break;
      }
      //SHR
      case 0x6: {
        chip8->Vregs[x] >>= 1;
        if (chip8->Vregs[x] % 2 == 1u)
          chip8->Vregs[0xFu] = 1u;
        else
          chip8->Vregs[0xFu] = 0;
        break;
      }
      //SUBN
      case 0x7: {
        chip8->Vregs[x] = chip8->Vregs[y] - chip8->Vregs[x];
        if (chip8->Vregs[y] > chip8->Vregs[x])
          chip8->Vregs[0xFu] = 1u;
        else
          chip8->Vregs[0xFu] = 0u;
        break;
      }
      //SHL
      case 0xE: {
        chip8->Vregs[x] = chip8->Vregs[x] * 2;
        if ((chip8->Vregs[x] & 0x80u) == 0x80u)
          chip8->Vregs[0xFu] = 1u;
        else
          chip8->Vregs[0xFu] = 0u;
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
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    const uint8_t y = (opcode & 0x00F0u) >> 4u;
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
    const uint16_t addr = opcode & 0x0FFFu;
    chip8->PC = addr + chip8->Vregs[0x0u];
    break;
  }
  case 0xC: {
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    const uint8_t num = opcode & 0x00FFu;
    uint8_t rand_num = rand() % 256; //rand() % (max - min + 1) + min; // NOLINT(cert-msc30-c, cert-msc50-cpp)
    rand_num = rand_num & num;
    chip8->Vregs[x] = rand_num;
    break;
  }
  case 0xD: {
    DRAW(chip8, opcode);
    break;
  }
    //DOWN = 1, UP = 0. Keyboard is mapped directly to value : key 1 is keyboard[1]
    //so if key 1 is pressed (down), keyboard[1] = 1
  case 0xE: {
    const uint8_t sub_op = opcode & 0xFFu;
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    if (sub_op == 0x9E) {
      if (chip8->keyboard[chip8->Vregs[x]] == 1u)
        chip8->PC += 2;
    }
    else if (sub_op == 0xA1u) {
      if (chip8->keyboard[chip8->Vregs[x]] == 0)
        chip8->PC += 2;
    }
    else {
      printf("Invalid opcode");
      printf("opcode = %04x\n", opcode);
      exit(EXIT_FAILURE);
    }
    break;
  }
  case 0xF: {
    const uint8_t sub_op = opcode & 0xFFu;
    const uint8_t x = (opcode & 0x0F00u) >> 8u;
    switch (sub_op) {
      case 0x07: {
        chip8->Vregs[x] = chip8->delay_timer;
        break;
      }
      //wait for keypress
      case 0x0A: {
        uint8_t key_value = chip8_keypress(chip8,getchar());
        if (key_value < 16) {
          chip8->keyboard[key_value] = 1u; //key was pressed
          chip8->Vregs[x] = key_value;
        }
        break;
      }
      case 0x15: {
        chip8->delay_timer = chip8->Vregs[x];
        break;
      }
      case 0x18: {
        chip8->sound_timer = chip8->Vregs[x];
        break;
      }
      case 0x1E: {
        chip8->Ireg = chip8->Vregs[x]+ chip8->Ireg;
        break;
      }
      case 0x29: {
        const uint8_t digit = chip8->Vregs[x];
        chip8->Ireg = FONTSET_ADDRESS+(5*digit); //5 byte per font
        //The value of I is set to the location for the hexadecimal sprite corresponding to the value of Vx
        break;
      }
      case 0x33: {
        uint8_t value = chip8->Vregs[x];
        chip8->mem[chip8->Ireg+2] = value % 10;
        value /= 10;
        chip8->mem[chip8->Ireg+1] = value % 10;
        value /= 10;
        chip8->mem[chip8->Ireg] = value % 10;
        break;
      }
      case 0x55: {
        for (uint8_t i = 0; i <= x; ++i)
        {
          chip8->mem[chip8->Ireg + i] = chip8->Vregs[i];
        }

        break;
      }
      case 0x65: {
        for (uint8_t i = 0; i <= x; ++i)
        {
          chip8->Vregs[i] = chip8->mem[chip8->Ireg + i];
        }
        break;
      }
    }
    break;
  }
  default: {
    printf("Invalid opcode");
    printf("opcode = %04x\n", opcode);
    exit(EXIT_FAILURE);
  }
  }
}

void chip8_clock_cycle(struct chip_8_ *chip8) {
  // Fetch opcode (16 bits) using program counter
  const uint16_t opcode =
      chip8->mem[chip8->PC] << 8 | chip8->mem[chip8->PC + 1];
  chip8->PC += 2;
  // Test print
  // printf("opcode: %04x\n", opcode);
  // Exec the code
  exec_instruction(chip8, opcode);
  draw_console(chip8);
  if (chip8->delay_timer > 0)
  {
    --chip8->delay_timer;
  }

  // Decrement the sound timer if it's been set
  if (chip8->sound_timer > 0)
  {
    --chip8->sound_timer;
  }
  //sound play, seems to work
  if (chip8->sound_timer > 0) {
    if (OS == "Linux") {
      const int fd = open("/dev/tty", O_WRONLY);
      if (fd != -1) {
        write(fd, "\a", 1);
        close(fd);
      }
    }
      else if (OS == "Windows") {
        Beep(2500, 166);
      }
      else
        printf("\a");
    }
}


/**
 *
 * @param argc arg count
 * @param argv should be <program> , <ROM file path>
 * @return 1 if exit_failure, 0 if exit_success
 */

//TODO: clock speed, display refresh rate
int main(const int argc, char **argv) {
  int exit_prog = 0;
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <ROM file>\n", argv[0]);
    exit(1);
  }
  struct chip_8_ *chip8 = chip8_new();

  printf("Loading ROM: %s\n", argv[1]);
  loadROM(chip8, argv[1]);
  /*Clear the terminal screen for program initialization
   *Alternatively
   *#define clear() printf("\033[H\033[J")
   */
  clear_console();
  // Main cpu loop below
  while (exit_prog == 0) {
    chip8_clock_cycle(chip8);
    //clock speed below
    clock_t init = clock();
    clock_t end = 0;
    while (end - init < 5) {
      end = clock();
    }
    end = 0;
    init = 0;
  }
  free(chip8);
  return 0;
}
