#ifndef UART_H
#define UART_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_init(unsigned long baud);
bool uart_get_command_nonblocking(char* buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif
