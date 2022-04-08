/* ----------------------------------------------------------------------------
 * @file serial.h
 * @brief  
 *
 * @author Jake Michael, jami1063@colorado.edu
 * @resources 
 * (+) Serial Programming HOWTO: 
 *    https://tldp.org/HOWTO/Serial-Programming-HOWTO/index.html
 * (+) Linux Serial Ports Using C/C++
 *    https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
 *---------------------------------------------------------------------------*/

#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

/* -- Baud Rate Settings -- 
 *  B50       50 baud
 *  B75       75 baud
 *  B110      110 baud
 *  B134      134.5 baud
 *  B150      150 baud
 *  B200      200 baud
 *  B300      300 baud
 *  B600      600 baud
 *  B1200     1200 baud
 *  B1800     1800 baud
 *  B2400     2400 baud
 *  B4800     4800 baud
 *  B9600     9600 baud
 *  B19200    19200 baud
 *  B38400    38400 baud
 *  B57600    57600 baud
 *  B76800    76800 baud
 *  B115200   115200 baud
 */

#define SERIAL_BAUD        (B115200)       // baud rate 
#define SERIAL_VTIME       (10)            // maximum blocking read time, in deciseconds 

/* 
 * @brief  A test communication routine 
 * @param  None
 * @return 0 on success, -1 on failure
 */
int serial_main_test();

/* 
 * @brief  Initializes the serial device for UART settings: 115200 baud, 8N1
 * @param  serial_device, the device filename, typically /dev/ttyS* or /dev/ttyACM* 
 * @return 0 on success, -1 on failure 
 */
int serial_init(char* serial_device);

/* 
 * @brief  Reads up to max_len bytes into buf
 * @param  char* buf, the buffer to store the read bytes
 *         len, the maximum number of bytes that could be read into buf
 * @return the number of bytes successfully read into buf, or a negative errno code
 */
int serial_read(char* buf, int len);


/* 
 * @brief  Writes len bytes into buf
 * @param  buf, the buffer to store the write bytes
 *         len, the number of bytes in buf to write to serial device
 * @return the number of bytes successfully read into buf, or a negative errno code
 */
int serial_write(const char* buf, int len);

/* 
 * @brief  Cleans up the serial port and memory associated with this module
 * @param  None
 * @return None
 */
void serial_cleanup();

#endif // #define _SERIAL_H_
