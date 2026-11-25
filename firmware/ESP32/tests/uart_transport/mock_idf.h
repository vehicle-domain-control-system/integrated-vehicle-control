#ifndef MOCK_IDF_H
#define MOCK_IDF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void mock_idf_reset(void);
void mock_idf_set_time_ms(uint64_t now_ms);
void mock_idf_fail_driver_install(bool fail);
void mock_idf_fail_task_create_call(unsigned call_number);
bool mock_idf_run_task(unsigned call_number);
void mock_idf_fail_uart_write(bool fail);
void mock_idf_fail_uart_wait(bool fail);
unsigned mock_idf_uart_write_count(void);
size_t mock_idf_last_uart_write(void *output, size_t output_capacity);
size_t mock_idf_live_queue_count(void);
size_t mock_idf_live_event_group_count(void);
bool mock_idf_driver_is_installed(void);
unsigned mock_idf_driver_delete_count(void);
bool mock_idf_queue_pop(unsigned creation_index, void *item, size_t item_size);
/* One-shot deterministic interleavings; never compiled into firmware. */
void mock_idf_after_tx_dequeue(void (*hook)(void));
void mock_idf_during_uart_write(void (*hook)(void));
void mock_idf_partial_uart_write(bool partial);

#endif
