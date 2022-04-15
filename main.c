/* =============================================================================
 * @file main.c
 * @brief MQTT client application which transmits and receives uart packets
 * +------------------------------------------------------------
 * @author Jake Michael, jami1063@colorado.edu
 * @usage: mqttify [-f serial_port] [-d]
 *    -f, --file : a serial port device to read/write data to (required)
 *    -d, --daemon :  run this process as a daemon
 *  example:
 *    mqttify --file /dev/ttyAMA0 --daemon
 *
 * @resources 
 *    https://github.com/Johannes4Linux/libmosquitto_examples
 *    https://github.com/eclipse/mosquitto/tree/master/examples
 * + ------------------------------------------------------------
 * ===========================================================================*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <mosquitto.h>
#include "log.h"
#include "serial.h"

#define BROKER    "ec36fe04c68947d399f3cbbc782e89ff.s2.eu.hivemq.cloud"
#define PORT      (8883)
#define KEEPALIVE (10)

//#define BROKER    "localhost"
//#define PORT      (8883)
//#define KEEPALIVE (10)

/*
 * The target expects password file located at /etc/mqttify/passwd.txt, vs. the
 * test/development environment has it in folder it is running from ./passwd.txt
 */
#ifdef TARGET_BUILD
  #pragma message ("TARGET_BUILD set")
  #define PASSWORD_FILE "/etc/mqttify/passwd.txt"
#else
  #define PASSWORD_FILE "passwd.txt"
#endif

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
void on_message(struct mosquitto *mosq, void* obj, const struct mosquitto_message *msg);


/*
 * @brief  Connect callback function
 * @param  struct mosquitto *mosq, the mosquitto instance
 *         void* obj, the user data provided in mosquitto_new
 *         rc, the result code of connection
 * @return None 
 */
void on_connect(struct mosquitto *mosq, void* obj, int rc);

/* 
 * @brief  Cleans up program and exits
 * @param  None
 * @return None
 */
static void cleanup_and_exit();



/* =============================================================================
 *    GLOBALS
 * ===========================================================================*/
static struct mosquitto *mosq = NULL; 
static bool global_abort = false;
static bool connection_status = false;


/* =============================================================================
 *    FUNCTION DEFINITIONS
 *============================================================================*/

int main(int argc, char** argv) 
{
  bool daemonize_flag = false;
  char* serial_device = NULL;
  int maxlen = 16;
  int rc = -1;

  // extract information from arguments
  if (argc > 1) 
  {
    for (int i=1; i<argc; i++)
    {
      if (strlen(argv[i]) > maxlen) continue;
      if ( !strcmp(argv[i], "-d") || !strcmp(argv[i],"--daemon") )
      {
        daemonize_flag = true;
      }
      else if ( (!strcmp(argv[i],"-f") || !strcmp(argv[i],"--file")) && i+1 < argc )
      {
         serial_device = argv[i+1];
      }
    }
  }
  
  printf("starting mqttify with serial_device = %s\n", serial_device);

  if (serial_device == NULL)
  {
    print_usage();
    return -1;
  }

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
  mosq = mosquitto_new( NULL, 
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

  mosquitto_connect_callback_set(mosq, on_connect);
  mosquitto_message_callback_set(mosq, on_message);

  FILE* password_file = NULL;
  password_file = fopen(PASSWORD_FILE, "r");
  if (!password_file)
  {
    LOG(LOG_ERR, "open(), password file not found at %s, %s", PASSWORD_FILE, 
         strerror(errno));
    goto cleanup;
  }

  size_t user_pass_len = 32; 
  char* username = NULL;
  char* password = NULL;
  bool in_username = true;
  char c;
  username = malloc(user_pass_len*sizeof(char));
  if (!username) 
  {
    LOG(LOG_ERR, "could not malloc username");
    goto cleanup;
  }
  password = malloc(user_pass_len*sizeof(char));
  if (!password) 
  {
    LOG(LOG_ERR, "could not malloc password");
    goto cleanup;
  }


  char *buf = username; // initially populate username 
  int i = 0;
  while(i < user_pass_len )
  {
    c = fgetc(password_file);
    *(buf++) = c; 
    i++;

    if (c == '\n' || c == '\0' || c == '\r')
    {
      *(buf-1) = '\0'; // terminate string
      if (in_username) 
      {
        buf = password;
        i = 0;
        in_username = false;
      }
      else 
      {
        break;
      }

    }
  }
  buf = NULL; // get rid of buf pointer


//  rc = mosquitto_tls_set( mosq,
//                          "/home/jmiv/aesd/MQTT-ify/cert.pem",
//                          NULL,
//                          "/home/jmiv/aesd/MQTT-ify/cert.pem",
//                          NULL,
//                          NULL
//                        );
  if (rc != MOSQ_ERR_SUCCESS) 
  {
    LOG(LOG_ERR, "mosquitto_tls_set rc=%d, %s", rc, mosquitto_strerror(rc));
  }

  // set username and password for authentication
  rc = mosquitto_username_pw_set( mosq,
                                  username, 
                                  password
                                );
  if (rc != MOSQ_ERR_SUCCESS) 
  {
    LOG(LOG_ERR, "mosquitto_username_pw_set rc=%d, %s", rc, mosquitto_strerror(rc));
  }
  memset(username, 0, user_pass_len); // write over memory
  free(username); username = NULL;
  memset(password, 0, user_pass_len); // write over memory
  free(password); password = NULL;

  // connect to broker
  rc = mosquitto_connect( mosq, 
                          BROKER, 
                          PORT, 
                          KEEPALIVE
                        );
  if (rc != MOSQ_ERR_SUCCESS)
  {
    LOG(LOG_ERR, "mosquitto_connect rc=%d, %s", rc, mosquitto_strerror(rc));
    goto cleanup;
  }

  // run the main (threaded) network loop for the client 
  //rc = mosquitto_loop_start(mosq);
  //if (rc != MOSQ_ERR_SUCCESS)
  //{
  //  LOG(LOG_ERR, "mosquitto_loop rc=%d, %s", rc, mosquitto_strerror(rc));
  //  goto cleanup;
  //}

  LOG(LOG_INFO, "waiting for connection...");
  const struct timespec sleep_time = {1, 0};
  while(!connection_status && !global_abort) 
  {
    nanosleep(&sleep_time, NULL);  // sleep for 1 second at a time
  };

  rc = mosquitto_publish( mosq, 
                          NULL, 
                          "mqttify/client-connection-status", 
                          sizeof(connection_status), 
                          &connection_status, 
                          0, 
                          false
                        );
  if (rc != MOSQ_ERR_SUCCESS)
  {
    LOG(LOG_ERR, "mosquitto_publish connection status rc=%d, %s", 
        rc, mosquitto_strerror(rc));
    goto cleanup;
  }

  if (daemonize_flag) 
  {
    rc = daemonize_proc();

    if (rc != 0)
    {
      LOG(LOG_ERR, "daemonize_proc rc=%d, %s", rc, mosquitto_strerror(rc));
      goto cleanup;
    }
  }

  int bytes_read;
  int rx_msg_len = 256;
  char* rx_msg = malloc(rx_msg_len*sizeof(char));
  if (!rx_msg)
  {
    LOG(LOG_ERR, "malloc rx_msg fail, %d", errno);
    goto cleanup;
  }


  while(!global_abort) 
  {
    memset(&rx_msg[0], 0, strlen(rx_msg));
    
    bytes_read = serial_read(rx_msg, rx_msg_len);
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
                              (void*) &rx_msg[0], 
                              0, 
                              false
                            );
      if (rc != MOSQ_ERR_SUCCESS)
      {
        LOG(LOG_ERR, "mosquitto_publish device-rx rc=%d, %s", \
            rc, mosquitto_strerror(rc));
        global_abort = true;
      }
    }

    rc = mosquitto_loop(mosq, 100, 10);
    if (rc != MOSQ_ERR_SUCCESS)
    {
      LOG(LOG_ERR, "mosquitto_loop rc=%d, %s", rc, mosquitto_strerror(rc));
      global_abort = true;
    }

  } // while(!global_abort)
  
cleanup:
  cleanup_and_exit();
  free(rx_msg);
  mosquitto_disconnect(mosq);
  mosquitto_loop_stop(mosq, true);

  return rc;
}

void cleanup_and_exit()
{
  mosquitto_disconnect(mosq);
  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();
}

void print_usage()
{
  printf("Usage: mqttify [-f serial_port] [-d | --daemon]\n");
  printf("required:\n");
  printf("  -f, --file : a uart serial device to read/write data to\n");
  printf("options: \n");
  printf("  -d, --daemon : run this process as a daemon\n");
  printf("example:\n");
  printf("  mqttify --file /dev/<example> -d\n");
}

void on_connect(struct mosquitto *mosq, void* obj, int rc)
{
  int ret;
  
  LOG(LOG_INFO, "on_connect, CONNACK: %d, %s", rc, mosquitto_connack_string(rc));
  if (rc != 0)
  {
    LOG(LOG_ERR, "on_connect rc=%d", rc);
    global_abort = true;
  }

  ret = mosquitto_subscribe(mosq, NULL, "mqttify/device-tx", 1);
  if (ret != 0)
  {
    LOG(LOG_ERR, "mosquitto_subscribe rc=%d, %s", rc, mosquitto_strerror(rc));
    global_abort = true;
  }
  connection_status = true;

} // on_connect


void on_message(struct mosquitto *mosq, void* obj, const struct mosquitto_message *msg)
{
  // write the message payload to tx
  serial_write((char*) msg->payload, msg->payloadlen);
} // on_message

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
