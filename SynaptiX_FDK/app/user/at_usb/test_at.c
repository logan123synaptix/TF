#include "test_at.h"
#include "logger.h"
#include "sx_board.h"
#include "ota_trigger.h"
#include "new_boot_backup_reg.h"
#include "new_magic_flash.h"

static const char *TAG = "TEST_AT_USB";

#define NUMBER_COMMAND  7

#define AT              0
#define AT_VPN          1
#define AT_MQTTCONNECT  2
#define AT_TIMESLEEP    3
#define AT_OTA          4
#define AT_ROLLBACK_PREV     5
#define AT_ROLLBACK_FACTORY  6

// const char* at_usb_command[NUMBER_COMMAND] = {"AT", "AT+VPN", "AT+MQTTCONNECT", "AT+TIMESLEEP", "AT+OTA", "AT+ROLLBACK_PREV", "AT+ROLLBACK_FACTORY"};

#define CMD_AT              "AT"
#define CMD_AT_VPN          "AT+VPN"
#define CMD_AT_MQTT         "AT+MQTTCONNECT"
#define CMD_AT_TIMESLEEP    "AT+TIMESLEEP"
#define CMD_AT_OTA          "AT+OTA"
#define CMD_AT_ROLLBACK_PREV     "AT+ROLLBACK_PREV"
#define CMD_AT_ROLLBACK_FACTORY  "AT+ROLLBACK_FACTORY"

#define AT_RESP_OK      "\r\nOK\r\n"
#define AT_RESP_ERROR   "\r\nERROR\r\n"

static void _respond(const char *resp)
{
    if (&board.usb == NULL) return;
    sx_usb_tiny_write(&board.usb, (const uint8_t *)resp, strlen(resp));
}

static int _at_execute(AT_Command_t *cmd)
{
    log_info(TAG, "Execute: %s", cmd->command);
    _respond(AT_RESP_OK);
    return 0;
}

static int _at_vpn_set(AT_Command_t *cmd, const char *param)
{
    log_info(TAG, "VPN set: %s", param ? param : "NULL");
    if (param == NULL) {
        _respond(AT_RESP_ERROR);
        return -1;
    }
    _respond(AT_RESP_OK);
    return 0;
}

static int _at_mqtt_set(AT_Command_t *cmd, const char *param)
{
    log_info(TAG, "MQTT set: %s", param ? param : "NULL");
    if (param == NULL) {
        _respond(AT_RESP_ERROR);
        return -1;
    }
    _respond(AT_RESP_OK);
    return 0;
}

static int _at_timesleep_set(AT_Command_t *cmd, const char *param)
{
    log_info(TAG, "TimeSleep set: %s", param ? param : "NULL");
    if (param == NULL) {
        _respond(AT_RESP_ERROR);
        return -1;
    }
    _respond(AT_RESP_OK);
    return 0;
}

/* AT+OTA: reboot into BOOTLOADER_WS's DFU/update mode. See ota_trigger.h
 * for the full contract — this only responds OK/ERROR based on whether the
 * flash write+verify succeeded; on success ota_trigger_enter_dfu() resets
 * the MCU itself and this function never returns to send the response. */
static int _at_ota_execute(AT_Command_t *cmd)
{
    log_info(TAG, "OTA: entering DFU/update mode");
    int rc = ota_trigger_enter_dfu();
    /* Only reached if ota_trigger_enter_dfu() failed (rc < 0) — on success
     * it resets the MCU before returning. */
    log_error(TAG, "OTA: ota_trigger_enter_dfu failed, rc=%d", rc);
    _respond(AT_RESP_ERROR);
    return rc;
}

/* AT+ROLLBACK_PREV / AT+ROLLBACK_FACTORY: write a one-shot command flag
 * into a TAMP backup register (survives NVIC_SystemReset(), see
 * new_boot_backup_reg.h) and reset. new_bootloader_check_commands(), run
 * early in the bootloader's bootloader_init(), reads the flag, clears it,
 * performs the swap (rollback-prev: Primary<->Secondary) or one-way copy
 * (rollback-factory: Factory->Primary), then jumps straight to the
 * application -- no DFU wait, no host tool involved.
 * Both always reset the MCU on this path, so neither ever returns to send
 * an AT response; the "OK"-then-reset ordering mirrors AT+OTA above. */
static int _at_rollback_prev_execute(AT_Command_t *cmd)
{
    log_info(TAG, "ROLLBACK_PREV: rolling back to previous (Secondary) app");
    _respond(AT_RESP_OK);
    boot_backup_reg_init();
    boot_backup_reg_write(BOOT_BACKUP_REG_ROLLBACK_PREV, BOOT_MAGIC_ROLLBACK_PREV);
    NVIC_SystemReset();
    return 0; /* unreachable */
}

static int _at_rollback_factory_execute(AT_Command_t *cmd)
{
    log_info(TAG, "ROLLBACK_FACTORY: restoring factory app image");
    _respond(AT_RESP_OK);
    boot_backup_reg_init();
    boot_backup_reg_write(BOOT_BACKUP_REG_ROLLBACK_FACTORY, BOOT_MAGIC_ROLLBACK_FACTORY);
    NVIC_SystemReset();
    return 0; /* unreachable */
}

static AT_Command_t s_commands[NUMBER_COMMAND] = {
    [AT] = {
    .command = CMD_AT,
    .handler = {
        .execute_handler  = _at_execute,  
        .question_handler = NULL,
        .set_handler      = NULL,
    }
    },
    [AT_VPN] = {
        .command = CMD_AT_VPN,
        .handler = {
            .set_handler      = _at_vpn_set,  
            .question_handler = NULL,
            .execute_handler  = NULL,
        }
    },
    [AT_MQTTCONNECT] = {
        .command = CMD_AT_MQTT,
        .handler = {
            .set_handler      = _at_mqtt_set, 
            .question_handler = NULL,
            .execute_handler  = NULL,
        }
    },
    [AT_TIMESLEEP] = {
        .command = CMD_AT_TIMESLEEP, 
        .handler = {
            .set_handler      = _at_timesleep_set, 
            .question_handler = NULL,
            .execute_handler  = NULL,
        }
    },
    [AT_OTA] = {
        .command = CMD_AT_OTA,
        .handler = {
            .execute_handler  = _at_ota_execute,
            .question_handler = NULL,
            .set_handler      = NULL,
        }
    },
    [AT_ROLLBACK_PREV] = {
        .command = CMD_AT_ROLLBACK_PREV,
        .handler = {
            .execute_handler  = _at_rollback_prev_execute,
            .question_handler = NULL,
            .set_handler      = NULL,
        }
    },
    [AT_ROLLBACK_FACTORY] = {
        .command = CMD_AT_ROLLBACK_FACTORY,
        .handler = {
            .execute_handler  = _at_rollback_factory_execute,
            .question_handler = NULL,
            .set_handler      = NULL,
        }
    },
};

static AT_Implementation_t s_at_impl;

void app_at_init(void) {
    at_init(&s_at_impl, s_commands, NUMBER_COMMAND);
    log_info(TAG, "AT command init OK");
}

void app_at_process(const char *data, size_t len) {
    at_process_input(&s_at_impl, data, len);
}