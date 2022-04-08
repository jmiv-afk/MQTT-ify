/* =============================================================================
 * @file main.c
 * @brief This is a mqtt client application which recieves uart packets 
 *
 * @author Jake Michael, jami1063@colorado.edu
 * @resources 
 * (+) example client:
 *    https://github.com/Johannes4Linux/libmosquitto_examples
 * ===========================================================================*/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mosquitto.h>
#include "log.h"
#include "serial.h"

/* =============================================================================
 *    FUNCTION HEADERS
 * ===========================================================================*/
/* 
 * @brief  Registers signal handlers
 * @param  None
 * @return 0 upon success, -1 on error
 */
static int register_signal_handlers();

/*
 * @brief   SIGINT and SIGTERM handler
 * @param   signo, the signal identifier
 * @return  None
 */
static void signal_handler(int signo);


/* @brief  turns the process into a daemon
 * @param  none
 * @return int, 0 on success, -1 on error
 */
static int daemonize_proc();

/* 
 * @brief prints the command line usage for the program
 */
void print_usage();

/*
 * @brief  Message callback function
 * @param  Struct mosquitto *mosq, the mosquitto instance
 *         void* obj, the user data provided in mosquitto_new
 *         mosquitto_message* msg, the message data, cleared after callback completes
 * @return None
 */
void message_cb(struct mosquitto *mosq, void* obj, const struct mosquitto_message *msg);


/*
 * @brief  Connect callback function
 * @param  struct mosquitto *mosq, the mosquitto instance
 *         void* obj, the user data provided in mosquitto_new
 *         rc, the result code of connection
 * @return None 
 */
void connect_cb(struct mosquitto *mosq, void* obj, int rc);

/* =============================================================================
 *    GLOBALS
 * ===========================================================================*/
static struct mosquitto *mosq = NULL; 
static bool global_abort = false;


/* =============================================================================
 *    FUNCTION DEFINITIONS
 *============================================================================*/

int main(int argc, char** argv) 
{
  bool connection_status = false;
  bool daemonize_flag = false;
  char* serial_device = NULL;
  int rc = -1;

  // extract information from arguments
  if (argc > 1) 
  {
    for (int i=1; i<argc; i++)
    {
      if ( !strcmp(argv[i], "-d") || !strcmp(argv[i],"--daemonize") )
      {
        daemonize_flag = true;
      }
      else if ( (!strcmp(argv[i],"-f") || !strcmp(argv[i],"--file")) && i+1 < argc )
      {
         serial_device = argv[i+1];
      }
    }
  }
  
  printf("serial_device = %s\n", serial_device);

  if (serial_device == NULL)
  {
    print_usage();
    return -1;
  }

  if (daemonize_flag) 
    daemonize_proc();

  // initialize the serial device
  if (0 != serial_init(serial_device)) 
  {
    serial_cleanup();
    LOG(LOG_ERR, "serial_init failed");
    return -1;
  }

  if(-1 == register_signal_handlers()) 
  {
    LOG(LOG_ERR, "register_signal_handlers fail");
  }
  
  // initialize the mosquitto library
  mosquitto_lib_init();

  // create the mosquitto client, with device_id "mqttify"
  mosq = mosquitto_new( "mqttify", 
                           true, 
                           NULL
                        );
  if (mosq == NULL)
  {
    LOG(LOG_ERR, "mosquitto_new fail, error=%s", strerror(errno));
    return -1;
  }

  // setup a will message in case of client disconnect
  rc = mosquitto_will_set( mosq,  
                           "mqttify/client-connection-status",
                           sizeof(connection_status),
                           &connection_status,
                           0,
                           false
                         );
  if (rc != 0)
  {
    LOG(LOG_ERR, "mosquitto_will_set rc=%d, %s", rc, mosquitto_strerror(rc));
  }

  mosquitto_connect_callback_set(mosq, connect_cb);
  mosquitto_message_callback_set(mosq, message_cb);

  // connect to broker
  rc = mosquitto_connect( mosq, 
                          "localhost", 
                          1883, 
                          60
                        );
  if (rc != 0)
  {
    LOG(LOG_ERR, "mosquitto_connect rc=%d, %s", rc, mosquitto_strerror(rc));
    mosquitto_destroy(mosq);
    return -1;
  }

  LOG(LOG_INFO, "client is connected to broker");
  connection_status = 1;
  rc = mosquitto_publish( mosq, 
                          NULL, 
                          "mqttify/client-connection-status", 
                          sizeof(connection_status), 
                          &connection_status, 
                          0, 
                          false
                        );
  if (rc != 0)
  {
    LOG(LOG_ERR, "mosquitto_publish rc=%d, %s", rc, mosquitto_strerror(rc));
    mosquitto_destroy(mosq);
    return -1;
  }

  int max_msglen = 256;
  char rx_msg[max_msglen];
  int bytes_read = 0;

  while(!global_abort) 
  {
    memset(&rx_msg[0], 0, 256);
    
    bytes_read = serial_read(rx_msg, max_msglen);
    if (bytes_read < 0)
    {
      LOG(LOG_ERR, "serial_read rc=%d", bytes_read);
      global_abort = true;
    }
    else if (bytes_read != 0)
    {
      // publish the message
      rc = mosquitto_publish( mosq, 
                              NULL, 
                              "mqttify/device-rx", 
                              strlen(rx_msg), 
                              (void*) &rx_msg, 
                              0, 
                              false
                            );
    }

    // run the main network loop for the client 
    mosquitto_loop();

  } // while(!global_abort)
  
  mosquitto_disconnect(mosq);
  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();

  // publish message
  return 0;
}

void print_usage()
{
  printf("Usage: mqttify [-required] [-options]\n");
  printf("Required:\n");
  printf("  -f, --file       a device file to read/write data to\n");
  printf("Options: \n");
  printf("  -d, --daemonize  run this process as a daemon\n");
  printf("Example:\n");
  printf("  mqttify --file /dev/<example> -d\n");
}

void connect_cb(struct mosquitto *mosq, void* obj, int rc)
{
  int ret;

  LOG(LOG_INFO, "connect_cb: %s", mosquitto_connack_string(rc));

  if (rc != 0)
  {
    LOG(LOG_ERR, "connect_cb rc=%d", rc);
    global_abort = true;
  }

  ret = mosquitto_subscribe(mosq, NULL, "mqttify/device-tx", 1);
  if (ret != 0)
  {
    LOG(LOG_ERR, "mosquitto_subscribe rc=%d, %s", rc, mosquitto_strerror(rc));
    global_abort = true;
  }

} // connect_cb


void message_cb(struct mosquitto *mosq, void* obj, const struct mosquitto_message *msg)
{
  // write the message payload to tx
  serial_write((char*) msg->payload, msg->payloadlen);
} // message_cb

static int register_signal_handlers() 
{
  // register signal handlers
  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    LOG(LOG_ERR, "cannot register SIGINT"); 
    return -1;
  }
  if (signal(SIGTERM, signal_handler) == SIG_ERR) {
    LOG(LOG_ERR, "cannot register SIGTERM"); 
    return -1;
  }
  return 0;
}

static void signal_handler(int signo)
{
  LOG(LOG_WARNING, "Caught signal, setting abort flag");
  global_abort = true;
} // end signal_handler


static int daemonize_proc() 
{
  // ignore signals
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid > 0) {
    // parent exits
    exit(EXIT_SUCCESS); 
  }

  // create a new session and set process group ID
  if (setsid() == -1) {
    perror("setsid()");
  }

  // change working directory to root
  chdir("/");

  // redirect STDIOs to /dev/null
  int devnull = open("/dev/null", O_RDWR);
  dup2(devnull, STDIN_FILENO);
  dup2(devnull, STDOUT_FILENO);
  dup2(devnull, STDERR_FILENO);
  close(devnull);
  return 0;
}
