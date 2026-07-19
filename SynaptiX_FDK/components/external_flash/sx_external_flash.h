#ifndef __SX_EXTERNAL_FLASH_H
#define __SX_EXTERNAL_FLASH_H

#ifdef  __cplusplus
extern "C"{
#endif

#include "stdint.h"
#include "stdbool.h"
#include "sx_gpio.h"
#include "logger.h" // DEBUG: needed for log_error added for hang investigation

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