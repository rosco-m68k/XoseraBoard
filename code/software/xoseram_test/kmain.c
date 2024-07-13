/*
 * Copyright (c) 2020 You (you@youremail.com)
 */

#include <stdio.h>
#include <stdint.h>
#include <machine.h>
#include <basicio.h>

#define REG_RAM_LOW     0xfc1060
#define REG_RAM_HIGH    0xfc1062

#define MEMCHECK_START  0x100000
#define MEMCHECK_END    0x200000

//#define ONLY_HIGH8
#define RUN_SIZE    0x100
//#define VALUE       (( 0xff - ((uint8_t)i) )) 
#define VALUE       0x08
//#define ONLY_ODD

//#define TEST_EACH_MEG
//#define TEST_BANK
#define TEST_LFSR

//#define SILENCE_ERRORS

void onboard_stack() {
}

void each_megabyte_test() {
    volatile uint8_t *ptr;
    int delay = 3;

    uint8_t value = 0;

    for (int addr = 0x100000; addr < 0xe00000; addr += 0x100000) {
        printf("\n\x1b[1;37mR/W test at base address 0x%08x\n", addr);
        ptr = (uint8_t*)addr;

        for (int i = 0; i < RUN_SIZE; i++) {
#ifdef ONLY_ODD
            ptr++;
#endif
#ifdef ONLY_HIGH8
            if ((i & 0x0f) < 8) {
                *ptr++ = 0;
            } else {
#endif
                *ptr = value;
                uint8_t check = *ptr;
                if (check != value) {
                    printf("\x1b[1;31m0x%02x write is bad: 0x%02x [LATEST VALUE 0x%02x]\x1b[0m\n", i, check, *ptr);
                }
                ptr++;
                delay ^= 1;
                mcDelaymsec10(delay);
#ifdef ONLY_HIGH8
            }
#endif
        }

        mcDelaymsec10(200);

        ptr = (uint8_t*)addr;

        for (int i = 0; i < RUN_SIZE; i++) {
            uint8_t val = *ptr++;

            if (val == value) {
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

            delay ^= 1;
            mcDelaymsec10(delay);
        }

        printf("\x1b[0m\n");

        value++;
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

/* memory testing routines */
static uint64_t start_state;
static uint32_t lfsr_val;

static void init_LFSR() {
  static uint32_t salt = 0xdeadbeef;
  do {
    start_state = _TIMER_100HZ;
    start_state += salt++;
    if (start_state > 0xffffffULL) {
      start_state++;
    }
    lfsr_val = (uint32_t)start_state & 0xffffff;
  } while (lfsr_val == 0);
}

static void reset_LFSR() {
  lfsr_val = (uint32_t)start_state & 0xffffff;
}

static inline uint32_t next_LFSR() {
  bool msb = lfsr_val & 0x800000; /* Get MSB (i.e., the output bit). */
  lfsr_val = lfsr_val << 1;       /* Shift register */
  if (msb)                        /* If the output bit is 1, */
    lfsr_val ^= 0x80042B;         /*  apply toggle mask. */
  lfsr_val &= 0xffffff;

  return lfsr_val;
}

void memcheck_bytes() {
  printf("Continuous testing from 0x%06x to 0x%06x (press a key to exit)\n",
         (unsigned int)MEMCHECK_START,
         (unsigned int)MEMCHECK_END - 1);

  int          memtest_words   = (MEMCHECK_END - MEMCHECK_START) / sizeof(uint8_t);
  uint8_t     *word_ptr        = (uint8_t *)MEMCHECK_START;
  int          update_interval = 0x7000;
  int          pass            = 1;
  int          badpasses       = 0;
  int          goodpasses      = 0;
  int          errors          = 0;
  unsigned int start_ts        = _TIMER_100HZ;
  for (;;) {
    bool pass_bad = false;

    init_LFSR();

    for (int i = 0; i < memtest_words; i += update_interval) {
      if (checkinput()) {
        break;
      }
      unsigned int ts = _TIMER_100HZ - start_ts;
      unsigned int tm = ts / (60 * 100);
      ts              = (ts - (tm * (60 * 100))) / 100;

      printf("\r%u:%02u %s Pass #%d - Filling bytes @ 0x%06x (LFSR 0x%02x)...    ",
             tm,
             ts,
             errors ? "[BAD]" : "[OK]",
             pass,
             (unsigned int)&word_ptr[i],
             (unsigned int)lfsr_val & 0xff);
      int k = memtest_words > (i + update_interval) ? (i + update_interval) : memtest_words;
      for (int j = i; j < k; j++) {
        uint8_t v  = next_LFSR();
        word_ptr[j] = v;
      }
    }

    if (checkinput()) {
      break;
    }

#if 0  // fake error testing
    if (pass > 2) {
      word_ptr[0xbaaad + pass] = 0xbaad;
    }
#endif

    reset_LFSR();

    for (int i = 0; i < memtest_words; i += update_interval) {
      if (checkinput()) {
        break;
      }
      unsigned int ts = _TIMER_100HZ - start_ts;
      unsigned int tm = ts / (60 * 100);
      ts              = (ts - (tm * (60 * 100))) / 100;
      printf("\r%u:%02u %s Pass #%d - Verifying bytes @ 0x%06x (LFSR 0x%02x)...",
             tm,
             ts,
             errors ? "[BAD]" : "[OK]",
             pass,
             (unsigned int)&word_ptr[i],
             (unsigned int)lfsr_val & 0xff);
      int k = memtest_words > (i + update_interval) ? (i + update_interval) : memtest_words;
      for (int j = i; j < k; j++) {
        uint8_t v = next_LFSR();
        if (word_ptr[j] != v) {
          errors++;
          if (!pass_bad) {
            badpasses++;
          }
          pass_bad = true;
#ifndef SILENCE_ERRORS
          printf("\r > Err %d, pass %d, 0x%06x=0x%04x vs 0x%04x expected                 \n",
                 errors,
                 pass,
                 (unsigned int)&word_ptr[j],
                 word_ptr[j],
                 v);
#endif
        }
      }
    }
    if (checkinput()) {
      break;
    }
    if (!pass_bad) {
      goodpasses++;
    }
    pass++;
  }
  inputchar();

  unsigned int ts = _TIMER_100HZ - start_ts;
  unsigned int tm = ts / (60 * 100);
  ts              = (ts - (tm * (60 * 100))) / 100;
  printf("\n\nTesting for %u:%02u, memcheck exiting...\n", tm, ts);
  if (errors) {
    printf(
        "FAILED! %d failed passes, %d good (%d word errors total).", badpasses, goodpasses, errors);
  } else if (goodpasses > 0) {
    printf("PASSED! %d error-free test passes.", goodpasses);
  }
}

void memcheck_words() {
  printf("Continuous testing from 0x%06x to 0x%06x (press a key to exit)\n",
         (unsigned int)MEMCHECK_START,
         (unsigned int)MEMCHECK_END - 1);

  int          memtest_words   = (MEMCHECK_END - MEMCHECK_START) / sizeof(uint16_t);
  uint16_t    *word_ptr        = (uint16_t *)MEMCHECK_START;
  int          update_interval = 0x7000;
  int          pass            = 1;
  int          badpasses       = 0;
  int          goodpasses      = 0;
  int          errors          = 0;
  unsigned int start_ts        = _TIMER_100HZ;
  for (;;) {
    bool pass_bad = false;

    init_LFSR();

    for (int i = 0; i < memtest_words; i += update_interval) {
      if (checkinput()) {
        break;
      }
      unsigned int ts = _TIMER_100HZ - start_ts;
      unsigned int tm = ts / (60 * 100);
      ts              = (ts - (tm * (60 * 100))) / 100;

      printf("\r%u:%02u %s Pass #%d - Filling words @ 0x%06x (LFSR 0x%04x)...    ",
             tm,
             ts,
             errors ? "[BAD]" : "[OK]",
             pass,
             (unsigned int)&word_ptr[i],
             (unsigned int)lfsr_val & 0xffff);
      int k = memtest_words > (i + update_interval) ? (i + update_interval) : memtest_words;
      for (int j = i; j < k; j++) {
        uint16_t v  = next_LFSR();
        word_ptr[j] = v;
      }
    }

    if (checkinput()) {
      break;
    }

#if 0  // fake error testing
    if (pass > 2) {
      word_ptr[0xbaaad + pass] = 0xbaad;
    }
#endif

    reset_LFSR();

    for (int i = 0; i < memtest_words; i += update_interval) {
      if (checkinput()) {
        break;
      }
      unsigned int ts = _TIMER_100HZ - start_ts;
      unsigned int tm = ts / (60 * 100);
      ts              = (ts - (tm * (60 * 100))) / 100;
      printf("\r%u:%02u %s Pass #%d - Verifying words @ 0x%06x (LFSR 0x%04x)...",
             tm,
             ts,
             errors ? "[BAD]" : "[OK]",
             pass,
             (unsigned int)&word_ptr[i],
             (unsigned int)lfsr_val & 0xffff);
      int k = memtest_words > (i + update_interval) ? (i + update_interval) : memtest_words;
      for (int j = i; j < k; j++) {
        uint16_t v = next_LFSR();
        if (word_ptr[j] != v) {
          errors++;
          if (!pass_bad) {
            badpasses++;
          }
          pass_bad = true;
#ifndef SILENCE_ERRORS
          printf("\r > Err %d, pass %d, 0x%06x=0x%04x vs 0x%04x expected                 \n",
                 errors,
                 pass,
                 (unsigned int)&word_ptr[j],
                 word_ptr[j],
                 v);
#endif
        }
      }
    }
    if (checkinput()) {
      break;
    }
    if (!pass_bad) {
      goodpasses++;
    }
    pass++;
  }
  inputchar();

  unsigned int ts = _TIMER_100HZ - start_ts;
  unsigned int tm = ts / (60 * 100);
  ts              = (ts - (tm * (60 * 100))) / 100;
  printf("\n\nTesting for %u:%02u, memcheck exiting...\n", tm, ts);
  if (errors) {
    printf(
        "FAILED! %d failed passes, %d good (%d word errors total).", badpasses, goodpasses, errors);
  } else if (goodpasses > 0) {
    printf("PASSED! %d error-free test passes.", goodpasses);
  }
}

void memcheck_long() {
  printf("Continuous testing from 0x%06x to 0x%06x (press a key to exit)\n",
         (unsigned int)MEMCHECK_START,
         (unsigned int)MEMCHECK_END - 1);

  int          memtest_words   = (MEMCHECK_END - MEMCHECK_START) / sizeof(uint32_t);
  uint32_t    *word_ptr        = (uint32_t *)MEMCHECK_START;
  int          update_interval = 0x7000;
  int          pass            = 1;
  int          badpasses       = 0;
  int          goodpasses      = 0;
  int          errors          = 0;
  unsigned int start_ts        = _TIMER_100HZ;
  for (;;) {
    bool pass_bad = false;

    init_LFSR();

    for (int i = 0; i < memtest_words; i += update_interval) {
      if (checkinput()) {
        break;
      }
      unsigned int ts = _TIMER_100HZ - start_ts;
      unsigned int tm = ts / (60 * 100);
      ts              = (ts - (tm * (60 * 100))) / 100;

      printf("\r%u:%02u %s Pass #%d - Filling longs @ 0x%06x (LFSR 0x%08lx)...    ",
             tm,
             ts,
             errors ? "[BAD]" : "[OK]",
             pass,
             (unsigned int)&word_ptr[i],
             lfsr_val);
      int k = memtest_words > (i + update_interval) ? (i + update_interval) : memtest_words;
      for (int j = i; j < k; j++) {
        uint32_t v = next_LFSR();
        word_ptr[j] = v;
      }
    }

    if (checkinput()) {
      break;
    }

#if 0  // fake error testing
    if (pass > 2) {
      word_ptr[0xbaaad + pass] = 0xbaad;
    }
#endif

    reset_LFSR();

    for (int i = 0; i < memtest_words >> 1; i += update_interval) {
      if (checkinput()) {
        break;
      }
      unsigned int ts = _TIMER_100HZ - start_ts;
      unsigned int tm = ts / (60 * 100);
      ts              = (ts - (tm * (60 * 100))) / 100;
      printf("\r%u:%02u %s Pass #%d - Verifying longs @ 0x%06x (LFSR 0x%08lx)...",
             tm,
             ts,
             errors ? "[BAD]" : "[OK]",
             pass,
             (unsigned int)&word_ptr[i],
             (unsigned long)lfsr_val);
      int k = memtest_words > (i + update_interval) ? (i + update_interval) : memtest_words;
      for (int j = i; j < k; j++) {
        uint32_t v = next_LFSR();
        if (word_ptr[j] != v) {
          errors++;
          if (!pass_bad) {
            badpasses++;
          }
          pass_bad = true;
#ifndef SILENCE_ERRORS
          printf("\r > Err %d, pass %d, 0x%06x=0x%08lx vs 0x%08lx expected                 \n",
                 errors,
                 pass,
                 (unsigned int)&word_ptr[j],
                 word_ptr[j],
                 v);
#endif
        }
      }
    }
    if (checkinput()) {
      break;
    }
    if (!pass_bad) {
      goodpasses++;
    }
    pass++;
  }
  inputchar();

  unsigned int ts = _TIMER_100HZ - start_ts;
  unsigned int tm = ts / (60 * 100);
  ts              = (ts - (tm * (60 * 100))) / 100;
  printf("\n\nTesting for %u:%02u, memcheck exiting...\n", tm, ts);
  if (errors) {
    printf(
        "FAILED! %d failed passes, %d good (%d word errors total).", badpasses, goodpasses, errors);
  } else if (goodpasses > 0) {
    printf("PASSED! %d error-free test passes.", goodpasses);
  }
}

void kmain() {
    printf("\x1b[1;37mXoseRAM \x1b[0mbringup tests\n");

#ifdef TEST_EACH_MEG
    each_megabyte_test();
#endif

#ifdef TEST_BANK
    bank_test();
#endif

#ifdef TEST_LFSR
    while (checkinput()) {  // clear any queued input
        inputchar();
    }
    memcheck_bytes();
#endif
}
