#ifndef MOCK_IDF_H
#define MOCK_IDF_H

#include <stdbool.h>
#include <stddef.h>

void mock_idf_reset(void);
void mock_idf_fail_driver_install(bool fail);
void mock_idf_fail_task_create_call(unsigned call_number);
size_t mock_idf_live_queue_count(void);
size_t mock_idf_live_event_group_count(void);
bool mock_idf_driver_is_installed(void);
unsigned mock_idf_driver_delete_count(void);
bool mock_idf_queue_pop(unsigned creation_index, void *item, size_t item_size);

#endif
