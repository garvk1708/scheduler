#include "uart.h"

static char rx_buffer[128];
static size_t rx_index = 0;

void custom_uart_init(unsigned long baud) {
    Serial.begin(baud);
    delay(10);
    rx_index = 0;
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

// Reads available bytes from Serial without blocking. Returns true and fills
// `buffer` when a newline-terminated command is complete.
// If the buffer fills before a newline arrives, the partial input is silently
// discarded and the buffer resets — keeps the loop moving rather than stalling.
bool uart_get_command_nonblocking(char* buffer, size_t max_len) {
    bool command_ready = false;
    
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                strncpy(buffer, rx_buffer, max_len);
                buffer[max_len - 1] = '\0';
                command_ready = true;
                rx_index = 0;
                break;
            }
        } else {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = c;
            } else {
                rx_index = 0; // overflow — drop and reset
            }
        }
    }
    
    return command_ready;
}
