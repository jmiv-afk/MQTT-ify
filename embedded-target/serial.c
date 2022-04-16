/* ----------------------------------------------------------------------------
 * @file serial.c
 * @brief  
 *
 * @author Jake Michael, jami1063@colorado.edu
 * @resources 
 * (+) Serial Programming HOWTO: 
 *    https://tldp.org/HOWTO/Serial-Programming-HOWTO/index.html
 * (+) Linux Serial Ports Using C/C++
 *    https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
 *---------------------------------------------------------------------------*/

#include "serial.h"
#include "log.h"

// the file descriptor associated with serial device
static int serial_device; 

int serial_main_test()
{
  if (0 != serial_init("/dev/pts/6")) 
  {
    serial_cleanup();
    return -1;
  }

  char read_buf [256];
  memset(read_buf, '\0', sizeof(read_buf));

  while(1) 
  {
    int num_bytes = serial_read(read_buf, sizeof(read_buf));
    if (num_bytes < 0) {
      LOG(LOG_ERR, "Error reading: %s\n", strerror(errno));
      break;
    }
    LOG(LOG_INFO, "Read %i bytes. Received message: %s\n", num_bytes, read_buf);
  }

  serial_cleanup();
  return 0; // success
}


int serial_init(char* serial_device_file)
{
  // open device
  serial_device = open(serial_device_file, O_RDWR); 
  if (serial_device < 0) 
  {
    LOG(LOG_ERR, "Error %i from open: %s\n", errno, strerror(errno));
  }

  // Create new termios struct attr
  struct termios attr;

  // Read existing settings, and handle any error
  // POSIX states that the struct passed to tcsetattr() must have been initialized 
  // with a call to tcgetattr() overwise behavior is undefined
  if(tcgetattr(serial_device, &attr) != 0) {
      LOG(LOG_ERR, "Error %i from tcgetattr: %s\n", errno, strerror(errno));
  }

  // set 8N1 (8 bits, no parity, 1 stop)
  attr.c_cflag &= ~PARENB;        // disable parity 
  attr.c_cflag &= ~CSTOPB;        // only one stop bit 
  attr.c_cflag &= ~CSIZE;         // clear all bits that set the data size 
  attr.c_cflag |= CS8;            // transmit 8 bits per character/packet
  attr.c_cflag &= ~CRTSCTS;       // disable RTS/CTS hardware flow control (most common)
  attr.c_cflag |= CREAD | CLOCAL; // turn on READ & ignore ctrl lines (CLOCAL = 1)
  attr.c_lflag &= ~ICANON;        // disable canonical mode
  attr.c_lflag &= ~(ECHO | ECHOE | ECHONL); // disable echoes
  attr.c_lflag &= ~ISIG;          // disable interpretation of INTR, QUIT and SUSP
  attr.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
  attr.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // disable special handling 
  attr.c_iflag |= IGNCR;   // ignore carriage returns
  attr.c_oflag &= ~OPOST;  // prevent special interpretation of output bytes 
  attr.c_oflag &= ~ONLCR;  // prevent conversion of newline to carriage return/line feed
  attr.c_cc[VTIME] = SERIAL_VTIME; // set max blocking read time in deciseconds 
  attr.c_cc[VMIN] = 0; // no minimum number of characters to read

  // Set in/out baud rates to be 115200
  cfsetispeed(&attr, SERIAL_BAUD);
  cfsetospeed(&attr, SERIAL_BAUD);

  // Save attr settings, also checking for error
  if (tcsetattr(serial_device, TCSANOW, &attr) != 0) {
      LOG(LOG_ERR, "Error %i from tcsetattr: %s\n", errno, strerror(errno));
      serial_cleanup();
      return -1;
  }

  return 0;
} // end serial_init


int serial_read(char *buf, int len) 
{
  // check for valid inputs
  if (buf == NULL) { return -EFAULT; }
  if (len <= 0) { return -EINVAL; }

  int nbytes = read(serial_device, buf, len);
  if (nbytes < 0) 
  {
    return -errno;
  }

  return nbytes;
} // end serial_read


int serial_write(const char* buf, int len) 
{
  // check for valid inputs
  if (buf == NULL) { return -EFAULT; }
  if (len <= 0) { return -EINVAL; }

  int nbytes = 0;
  while(len) {
    nbytes = write(serial_device, buf, len);
    if (nbytes < 0) {
      return -errno;
    }
    len -= nbytes;
    buf += nbytes;
  }

  return 0; 
} // end serial_write

void serial_cleanup()
{
  close(serial_device);
} // end serial_cleanup
