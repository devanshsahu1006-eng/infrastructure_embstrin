#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOSConfig.h"

// ---- IR Opcodes ----
#define OP_GET_DEVICE_INFO      0x01
#define OP_GET_RAM_INFO         0x02
#define OP_GET_FLASH_INFO       0x03
#define OP_GET_DEVICE_NAME      0x05
#define OP_SEND_DATA            0x10
#define OP_RECV_DATA            0x11
#define OP_ADC_READ             0x23
#define OP_PWM_START            0x24
#define OP_UART_SEND            0x25
#define OP_GET_CPU_USAGE        0x30
#define OP_GET_FREE_RAM         0x31
#define OP_GET_TEMPERATURE      0x32
#define OP_GET_ACTIVE_TASKS     0x33

// ---- Constants ----
#define USB_BUF_SIZE            256
#define UART_BUF_SIZE           256
#define USB_TIMEOUT_MS          100
#define HOST_DEVICE_ID          0x00
#define THIS_DEVICE_ID          0x01   

// ---- UART config for device-to-device ----
#define INTER_DEVICE_UART_NUM   UART_NUM_1
#define INTER_DEVICE_UART_TX    4
#define INTER_DEVICE_UART_RX    5
#define INTER_DEVICE_BAUD       115200

// ---- Forward declarations ----
static void init_usb(void);
static void init_uart(void);
static void send_hello(void);
static void dispatch(uint8_t* buf, int len);

// device info handlers
static void handle_get_device_info(void);
static void handle_get_ram_info(void);
static void handle_get_flash_info(void);
static void handle_get_device_name(void);

// io handlers
static void handle_send_data(uint8_t* operands, uint8_t operand_count);
static void handle_recv_data(uint8_t* operands, uint8_t operand_count);

// capability handlers
static void handle_adc_read(uint8_t pin);
static void handle_pwm_start(uint8_t* operands, uint8_t operand_count);
static void handle_uart_send(uint8_t* operands, uint8_t operand_count);

// runtime handlers
static void handle_get_cpu_usage(void);
static void handle_get_free_ram(void);
static void handle_get_temperature(void);
static void handle_get_active_tasks(void);

// helpers
static void send_response(uint8_t* data, int len);
static void handle_error(uint8_t opcode);

// -----------------------------------------------
// app_main — entry point
// -----------------------------------------------
void app_main(void)
{
    // init hardware
    init_usb();
    init_uart();

    // tell host we're alive
    send_hello();

    // command loop — runs forever
    uint8_t buf[USB_BUF_SIZE];
    while (1) {
        int len = usb_serial_jtag_read_bytes(
                      buf, sizeof(buf),
                      USB_TIMEOUT_MS / portTICK_PERIOD_MS);
        if (len > 0) {
            dispatch(buf, len);
        }
    }
}

// -----------------------------------------------
// Init USB
// -----------------------------------------------
static void init_usb(void)
{
    usb_serial_jtag_driver_config_t cfg =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&cfg);
    printf("USB initialized\n");
}

// -----------------------------------------------
// Init UART (for device-to-device communication)
// -----------------------------------------------
static void init_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate  = INTER_DEVICE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE
    };
    uart_driver_install(INTER_DEVICE_UART_NUM,
                        UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(INTER_DEVICE_UART_NUM, &uart_config);
    uart_set_pin(INTER_DEVICE_UART_NUM,
                 INTER_DEVICE_UART_TX,
                 INTER_DEVICE_UART_RX,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    printf("UART initialized\n");
}

// -----------------------------------------------
// Send HELLO to host
// -----------------------------------------------
static void send_hello(void)
{
    // TODO: replace with proper protocol packet once
    // Sarvachaure/Prateek finalize HELLO packet format
    uint8_t hello[64];
    snprintf((char*)hello, sizeof(hello),
             "HELLO:device_id=%d:type=ESP32C6\n", THIS_DEVICE_ID);
    send_response(hello, strlen((char*)hello));
}

// -----------------------------------------------
// Dispatch
// -----------------------------------------------
static void dispatch(uint8_t* buf, int len)
{
    // need at least opcode + operand_count
    if (len < 2) {
        handle_error(0xFF);
        return;
    }

    uint8_t  opcode        = buf[0];
    uint8_t  operand_count = buf[1];
    uint8_t* operands      = &buf[2];

    // validate operands fit in buffer
    if (2 + operand_count > len) {
        handle_error(opcode);
        return;
    }

    switch (opcode) {
        // ---- device info ----
        case OP_GET_DEVICE_INFO:
            handle_get_device_info();
            break;
        case OP_GET_RAM_INFO:
            handle_get_ram_info();
            break;
        case OP_GET_FLASH_INFO:
            handle_get_flash_info();
            break;
        case OP_GET_DEVICE_NAME:
            handle_get_device_name();
            break;

        // ---- io ----
        case OP_SEND_DATA:
            handle_send_data(operands, operand_count);
            break;
        case OP_RECV_DATA:
            handle_recv_data(operands, operand_count);
            break;

        // ---- capabilities ----
        case OP_ADC_READ:
            if (operand_count < 1) { handle_error(opcode); return; }
            handle_adc_read(operands[0]);
            break;
        case OP_PWM_START:
            handle_pwm_start(operands, operand_count);
            break;
        case OP_UART_SEND:
            handle_uart_send(operands, operand_count);
            break;

        // ---- runtime ----
        case OP_GET_CPU_USAGE:
            handle_get_cpu_usage();
            break;
        case OP_GET_FREE_RAM:
            handle_get_free_ram();
            break;
        case OP_GET_TEMPERATURE:
            handle_get_temperature();
            break;
        case OP_GET_ACTIVE_TASKS:
            handle_get_active_tasks();
            break;

        default:
            handle_error(opcode);
            break;
    }
}

// -----------------------------------------------
// Device info handlers
// -----------------------------------------------
static void handle_get_device_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint8_t response[128];
    snprintf((char*)response, sizeof(response),
             "DEVICE_INFO:id=%d:cores=%d:revision=%d\n",
             THIS_DEVICE_ID,
             chip_info.cores,
             chip_info.revision);
    send_response(response, strlen((char*)response));
}

static void handle_get_ram_info(void)
{
    uint32_t total_ram = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    uint32_t free_ram  = heap_caps_get_free_size(MALLOC_CAP_8BIT);

    uint8_t response[64];
    snprintf((char*)response, sizeof(response),
             "RAM_INFO:total=%lu:free=%lu\n",
             total_ram, free_ram);
    send_response(response, strlen((char*)response));
}

static void handle_get_flash_info(void)
{
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    uint8_t response[64];
    snprintf((char*)response, sizeof(response),
             "FLASH_INFO:size=%lu\n", flash_size);
    send_response(response, strlen((char*)response));
}

static void handle_get_device_name(void)
{
    uint8_t response[] = "DEVICE_NAME:ESP32C6\n";
    send_response(response, sizeof(response));
}

// -----------------------------------------------
// IO handlers
// -----------------------------------------------
static void handle_send_data(uint8_t* operands, uint8_t operand_count)
{
    if (operand_count < 3) { handle_error(OP_SEND_DATA); return; }

    uint8_t  dest_id   = operands[0];
    uint16_t data_size = (operands[1] << 8) | operands[2];
    uint8_t* data      = &operands[3];

    if (dest_id == HOST_DEVICE_ID) {
        // send back to host over USB
        send_response(data, data_size);
    } else {
        // forward to another device over UART
        uart_write_bytes(INTER_DEVICE_UART_NUM,
                         (const char*)data, data_size);
    }
}

static void handle_recv_data(uint8_t* operands, uint8_t operand_count)
{
    if (operand_count < 2) { handle_error(OP_RECV_DATA); return; }

    uint16_t expected_size = (operands[0] << 8) | operands[1];

    uint8_t recv_buf[UART_BUF_SIZE];
    int len = uart_read_bytes(INTER_DEVICE_UART_NUM,
                               recv_buf,
                               expected_size,
                               USB_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (len > 0) {
        send_response(recv_buf, len);
    } else {
        uint8_t response[] = "RECV_DATA:timeout\n";
        send_response(response, sizeof(response));
    }
}

// -----------------------------------------------
// Capability handlers
// -----------------------------------------------
static void handle_adc_read(uint8_t pin)
{
    // TODO: configure ADC channel properly once
    // Chinmay finalizes which pins are ADC capable
    // For now returns placeholder value
    uint8_t response[64];
    snprintf((char*)response, sizeof(response),
             "ADC:pin=%d:val=0\n", pin);
    send_response(response, strlen((char*)response));
}

static void handle_pwm_start(uint8_t* operands, uint8_t operand_count)
{
    if (operand_count < 3) { handle_error(OP_PWM_START); return; }

    uint8_t  pin       = operands[0];
    uint16_t frequency = (operands[1] << 8) | operands[2];

    // TODO: configure LEDC peripheral properly
    // LEDC is ESP32's PWM controller
    uint8_t response[64];
    snprintf((char*)response, sizeof(response),
             "PWM:pin=%d:freq=%d:started\n", pin, frequency);
    send_response(response, strlen((char*)response));
}

static void handle_uart_send(uint8_t* operands, uint8_t operand_count)
{
    if (operand_count < 3) { handle_error(OP_UART_SEND); return; }

    uint16_t data_size = (operands[0] << 8) | operands[1];
    uint8_t* data      = &operands[2];

    uart_write_bytes(INTER_DEVICE_UART_NUM,
                     (const char*)data, data_size);

    uint8_t response[] = "UART_SEND:ok\n";
    send_response(response, sizeof(response));
}

// -----------------------------------------------
// Runtime handlers
// -----------------------------------------------
static void handle_get_cpu_usage(void)
{
    // TODO: use FreeRTOS vTaskGetRunTimeStats for real value
    // requires configGENERATE_RUN_TIME_STATS = 1 in FreeRTOSConfig.h
    uint8_t response[] = "CPU_USAGE:0%\n";
    send_response(response, sizeof(response));
}

static void handle_get_free_ram(void)
{
    uint32_t free_ram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint8_t response[64];
    snprintf((char*)response, sizeof(response),
             "FREE_RAM:%lu\n", free_ram);
    send_response(response, strlen((char*)response));
}

static void handle_get_temperature(void)
{
    // TODO: use ESP32C6 internal temperature sensor API
    // driver/temperature_sensor.h once Chinmay confirms it's needed
    uint8_t response[] = "TEMPERATURE:0C\n";
    send_response(response, sizeof(response));
}

static void handle_get_active_tasks(void)
{
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    uint8_t response[64];
    snprintf((char*)response, sizeof(response),
             "ACTIVE_TASKS:%d\n", task_count);
    send_response(response, strlen((char*)response));
}

// -----------------------------------------------
// Helpers
// -----------------------------------------------
static void send_response(uint8_t* data, int len)
{
    usb_serial_jtag_write_bytes(data, len,
        USB_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static void handle_error(uint8_t opcode)
{
    uint8_t response[32];
    snprintf((char*)response, sizeof(response),
             "ERROR:opcode=0x%02X\n", opcode);
    send_response(response, strlen((char*)response));
}