/*
 * Copyright (c) 2020 You (you@youremail.com)
 */

#include <stdio.h>
#include <stdint.h>
#include <machine.h>

#define REG_RAM_LOW     0xfc1060
#define REG_RAM_HIGH    0xfc1062

//#define ONLY_HIGH8
#define RUN_SIZE    0x100

void each_megabyte_test() {
    volatile uint8_t *ptr;
    
    for (int addr = 0x100000; addr < 0xe00000; addr += 0x100000) {
        printf("\n\x1b[1;37mR/W test at base address 0x%08x\n", addr);
        ptr = (uint8_t*)addr;

        for (int i = 0; i < RUN_SIZE; i++) {
#ifdef ONLY_HIGH8
            if ((i & 0x0f) < 8) {
                *ptr++ = 0;
            } else {
#endif
                *ptr = 0xff - ((uint8_t)i);
                uint8_t check = *ptr;
                if (check != 0xff - ((uint8_t)i)) {
                    printf("\x1b[1;31m0x%02x write is bad: 0x%02x [LATEST VALUE 0x%02x]\x1b[0m\n", i, check, *ptr);
                }
                ptr++;
                mcDelaymsec10(2);
#ifdef ONLY_HIGH8
            }
#endif
        }

        mcDelaymsec10(200);

        ptr = (uint8_t*)addr;

        for (int i = 0; i < RUN_SIZE; i++) {
            uint8_t val = *ptr++;

            if (val == 0xff - ((uint8_t)i)) {
                printf("\x1b[1;32m");
            } else {
                printf("\x1b[1;31m");
            }

            printf("0x%02x", val);

            if ((i + 1) % 16 == 0) {
                printf("\n");
            } else {
                printf("  ");
            }

            mcDelaymsec10(3);
        }

        printf("\x1b[0m\n");
    }
}

void bank_test() {
    // Write bottom byte of each MB in each bank
    // Write to set low bank
    volatile uint8_t *ptr = (uint8_t*)REG_RAM_LOW;
    *ptr = 1;

    for (int addr = 0x100000; addr < 0xe00000; addr += 0x100000) {
        printf("\x1b[1;37mLow  bank write at address 0x%08x\n", addr);
        ptr = (uint8_t*)addr;
        *ptr = 0xa0;
    }

    // Write to set high bank
    ptr = (uint8_t*)REG_RAM_HIGH;
    *ptr = 1;
    
    for (int addr = 0x100000; addr < 0xe00000; addr += 0x100000) {
        printf("\x1b[1;37mHigh bank write at address 0x%08x\n", addr);
        ptr = (uint8_t*)addr;
        *ptr = 0xc0;
    }

    // Read back and check
    printf("\n");
    // Switch back to low bank
    ptr = (uint8_t*)REG_RAM_LOW;
    *ptr = 1;

    for (int addr = 0x100000; addr < 0xe00000; addr += 0x100000) {
        printf("\x1b[1;37mLow  bank check at address 0x%08x: ", addr);
        ptr = (uint8_t*)addr;
        if (*ptr == 0xa0) {
            printf("\x1b[1;32mOK   0x%02x\x1b[0m\n", *ptr);
        } else {
            printf("\x1b[1;31mFAIL 0x%02x\x1b[0m\n", *ptr);
        }
    }

    // Switch back to high bank
    ptr = (uint8_t*)REG_RAM_HIGH;
    *ptr = 1;

    for (int addr = 0x100000; addr < 0xe00000; addr += 0x100000) {
        printf("\x1b[1;37mHigh bank check at address 0x%08x: ", addr);
        ptr = (uint8_t*)addr;
        if (*ptr == 0xc0) {
            printf("\x1b[1;32mOK   0x%02x\x1b[0m\n", *ptr);
        } else {
            printf("\x1b[1;31mFAIL 0x%02x\x1b[0m\n", *ptr);
        }
    }
}

void kmain() {
    printf("\x1b[1;37mXoseRAM \x1b[0mbringup tests\n");

    each_megabyte_test();
    bank_test();
}
