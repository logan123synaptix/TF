#ifndef __SX_EXTERNAL_FLASH_H
#define __SX_EXTERNAL_FLASH_H

#ifdef  __cplusplus
extern "C"{
#endif

#include "stdint.h"
#include "stdbool.h"
#include "sx_gpio.h"
#include "logger.h" // DEBUG: needed for log_error added for hang investigation
#include "stm32h5xx_hal.h" // DEBUG: pulls in CMSIS core header for __get_MSP(), used for stack-check trace below

// DEBUG: linker-provided symbol for top of RAM stack (see STM32H563xx_FLASH.ld / _RAM.ld).
// This is an address, not a variable -- always take its address with '&'.
extern uint32_t _estack;

// DEBUG: NOT read from &_Min_Stack_Size on purpose. In the .ld file that symbol is only
// ever used inside a size computation ("_Min_Stack_Size" bytes), not as a real linker
// symbol meant to have its address taken -- doing so could silently read garbage.
// Hardcoded here to mirror the current linker script value; UPDATE THIS if _Min_Stack_Size
// is changed again in STM32H563xx_FLASH.ld / _RAM.ld (currently 0x1000, changed from 0x600).
#define DEBUG_MIN_STACK_SIZE 0x1000u

typedef struct {
    uint32_t page_size;     /* bytes per page   */
    uint32_t sector_size;   /* bytes per sector */
    uint32_t block_size;    /* bytes per block  */
    uint32_t total_bytes;   /* total chip size  */
} sx_ext_flash_info_t;

typedef struct {
    int  (*read)        (uint32_t addr, uint8_t *buf, uint32_t len);
    int  (*write)       (uint32_t addr, const uint8_t *buf, uint32_t len);
    int  (*erase_sector)(uint32_t addr);
    bool (*is_busy)     (void);
} sx_ext_flash_ops_t;

typedef struct {
    sx_ext_flash_ops_t  *ops;
    sx_ext_flash_info_t  info;
} sx_ext_flash_t;

static inline void sx_ext_flash_init(sx_ext_flash_t *flash, sx_ext_flash_ops_t *ops, sx_ext_flash_info_t *info){
    flash->ops = ops;
    flash->info = *info;
}

static inline int sx_ext_flash_read(sx_ext_flash_t *flash, uint32_t addr, uint8_t *buf, uint32_t len)
{
    // DEBUG: temporary trace to locate the hang after boot-via-bootloader.
    // Confirms whether flash->ops / flash->ops->read still look sane right before
    // jumping through the function pointer.
    log_error("SX_EXT_FLASH", "read: flash=%p ops=%p read_fn=%p addr=0x%06lX len=%lu",
        (void*)flash, flash ? (void*)flash->ops : (void*)0,
        (flash && flash->ops) ? (void*)flash->ops->read : (void*)0,
        (unsigned long)addr, (unsigned long)len);

    // DEBUG: stack-check trace. Measures the real MSP right before the function-pointer
    // call that never seems to land inside sx_W25Q128_read(). Compares against _estack
    // (top of RAM stack) and the reserved _Min_Stack_Size floor, to confirm or rule out
    // stack overflow as the cause of the hang. Read alongside sx_ext_flash.h's other
    // DEBUG logs; remove once root cause is confirmed (see handoff "don't forget to clean up").
    {
        uint32_t sp = __get_MSP();
        uint32_t estack_addr = (uint32_t)&_estack;
        uint32_t min_stack = DEBUG_MIN_STACK_SIZE;
        uint32_t used = estack_addr - sp;
        int32_t margin_to_floor = (int32_t)(sp - (estack_addr - min_stack));
        log_error("SX_EXT_FLASH", "SP_CHECK: sp=0x%08lX estack=0x%08lX min_stack=%lu used=%lu margin_to_floor=%ld",
            (unsigned long)sp, (unsigned long)estack_addr, (unsigned long)min_stack,
            (unsigned long)used, (long)margin_to_floor);
    }

    if (flash->ops && flash->ops->read)
        return flash->ops->read(addr, buf, len);
    return -1;
}

static inline int sx_ext_flash_write(sx_ext_flash_t *flash, uint32_t addr, const uint8_t *buf, uint32_t len){
    if (flash->ops && flash->ops->write)
        return flash->ops->write(addr, buf, len);
    return -1;
}

static inline int sx_ext_flash_erase_sector(sx_ext_flash_t *flash, uint32_t addr)
{
    if (flash->ops && flash->ops->erase_sector)
        return flash->ops->erase_sector(addr);
    return -1;
}

static inline bool sx_ext_flash_is_busy(sx_ext_flash_t *flash)
{
    if (flash->ops && flash->ops->is_busy)
        return flash->ops->is_busy();
    return false;
}

/*API*/

#ifdef  __cplusplus
}
#endif

#endif