#include "app_tasks.h"

#include "app_port.h"
#include "modbus_rtu.h"
#include "terminal.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include <string.h>

#define SAMPLE_PERIOD_MS       1000u
#define ALARM_PERIOD_MS         250u
#define MODBUS_RX_TIMEOUT_MS    100u
#define MODBUS_TX_RETRIES         3u
#define LOG_QUEUE_LENGTH         16u
#define ACQUIRE_STACK_WORDS     384u
#define COMM_STACK_WORDS        512u
#define STORAGE_STACK_WORDS     512u
#define ALARM_STACK_WORDS       256u

typedef struct {
    terminal_sample_t sample;
    terminal_thresholds_t thresholds;
} app_state_t;

static app_state_t app_state;
static SemaphoreHandle_t state_mutex;
static QueueHandle_t log_queue;

static void acquire_task(void *argument)
{
    terminal_sample_t sample;
    TickType_t last_wake = xTaskGetTickCount();
    (void)argument;

    for (;;) {
        memset(&sample, 0, sizeof(sample));
        if (app_port_read_sensors(&sample)) {
            if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
                app_state.sample = sample;
                xSemaphoreGive(state_mutex);
            }

            if (xQueueSend(log_queue, &sample, 0u) != pdPASS) {
                terminal_sample_t discarded;
                (void)xQueueReceive(log_queue, &discarded, 0u);
                (void)xQueueSend(log_queue, &sample, 0u);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

static void communication_task(void *argument)
{
    uint8_t request[MODBUS_RTU_MAX_ADU];
    uint8_t response[MODBUS_RTU_MAX_ADU];
    size_t request_length;
    size_t response_length;
    uint8_t retries;
    modbus_rtu_state_t local_state;
    (void)argument;

    for (;;) {
        request_length = 0u;
        if (!app_port_receive_modbus(request, sizeof(request), &request_length,
                                     MODBUS_RX_TIMEOUT_MS)) {
            continue;
        }

        if (xSemaphoreTake(state_mutex, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        local_state.sample = app_state.sample;
        local_state.thresholds = app_state.thresholds;
        xSemaphoreGive(state_mutex);

        response_length = modbus_rtu_process(&local_state, request, request_length,
                                             response, sizeof(response));
        if (response_length == 0u) {
            continue;
        }

        if (request_length >= 2u && (request[1] == 0x06u || request[1] == 0x10u) &&
            (response[1] == request[1])) {
            if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
                app_state.thresholds = local_state.thresholds;
                xSemaphoreGive(state_mutex);
            }
        }
        for (retries = 0u; retries < MODBUS_TX_RETRIES; ++retries) {
            if (app_port_send_modbus(response, response_length)) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10u));
        }
    }
}

static void storage_task(void *argument)
{
    terminal_sample_t sample;
    terminal_thresholds_t thresholds;
    terminal_alarm_t alarm;
    (void)argument;

    for (;;) {
        if (xQueueReceive(log_queue, &sample, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
            thresholds = app_state.thresholds;
            xSemaphoreGive(state_mutex);
        } else {
            continue;
        }
        alarm = terminal_evaluate_alarms(&sample, &thresholds);
        (void)app_port_append_log(&sample, &alarm);
    }
}

static void alarm_task(void *argument)
{
    terminal_sample_t sample;
    terminal_thresholds_t thresholds;
    terminal_alarm_t alarm;
    TickType_t last_wake = xTaskGetTickCount();
    (void)argument;

    for (;;) {
        if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
            sample = app_state.sample;
            thresholds = app_state.thresholds;
            xSemaphoreGive(state_mutex);
            alarm = terminal_evaluate_alarms(&sample, &thresholds);
            app_port_set_alarm_outputs(&alarm);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ALARM_PERIOD_MS));
    }
}

BaseType_t app_tasks_create(void)
{
    memset(&app_state, 0, sizeof(app_state));
    terminal_default_thresholds(&app_state.thresholds);

    state_mutex = xSemaphoreCreateMutex();
    log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(terminal_sample_t));
    if (state_mutex == NULL || log_queue == NULL) {
        return pdFAIL;
    }

    if (xTaskCreate(acquire_task, "Acquire", ACQUIRE_STACK_WORDS, NULL,
                    tskIDLE_PRIORITY + 3u, NULL) != pdPASS ||
        xTaskCreate(communication_task, "Modbus", COMM_STACK_WORDS, NULL,
                    tskIDLE_PRIORITY + 3u, NULL) != pdPASS ||
        xTaskCreate(storage_task, "Storage", STORAGE_STACK_WORDS, NULL,
                    tskIDLE_PRIORITY + 1u, NULL) != pdPASS ||
        xTaskCreate(alarm_task, "Alarm", ALARM_STACK_WORDS, NULL,
                    tskIDLE_PRIORITY + 2u, NULL) != pdPASS) {
        return pdFAIL;
    }
    return pdPASS;
}
