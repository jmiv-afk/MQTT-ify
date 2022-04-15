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
 * + ------------------------------------------------------------
 * ===========================================================================*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "MQTTClient.h"
#include "log.h"
#include "serial.h"

#define BROKER  ("ssl://ec36fe04c68947d399f3cbbc782e89ff.s2.eu.hivemq.cloud:8883")
#define KEEPALIVE (10)

// QOS for client 
// QOS 0, fire and forget - message may not be delivered
// QOS 1, message is delivered but may be delivered more than once
// QOS 2, message is delivered but may be delivered more than once
#define QOS       (1) 

//#define BROKER    "tcp://localhost:8883"
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
 * @brief  Cleans up program and exits
 * @param  None
 * @return None
 */
static void cleanup_and_exit();

void disconnect_cb(void *context, char* cause);

int message_cb(void *context, char* topicName, int topicLen, MQTTClient_message *message);

/* =============================================================================
 *    GLOBALS
 * ===========================================================================*/
MQTTClient client;
static bool global_abort = false;
static bool try_reconnect = false;


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
  
  // configure and create the MQTT Client
  MQTTClient client;
  rc = MQTTClient_create(&client, BROKER, "mqttifyremote", 
                         MQTTCLIENT_PERSISTENCE_NONE, NULL);
  if ( MQTTCLIENT_SUCCESS != rc)
  {
    LOG(LOG_ERR, "MQTTClient_create() returned %d", rc);
    goto cleanup;
  }

  MQTTClient_setCallbacks(client, NULL, disconnect_cb, message_cb, NULL);
  if ( MQTTCLIENT_SUCCESS != rc)
  {
    LOG(LOG_ERR, "MQTTClient_setCallbacks() returned %d", rc);
    goto cleanup;
  }

  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
  ssl_opts.enableServerCertAuth = 0;
  ssl_opts.verify = 1;
  ssl_opts.CApath = NULL;
  ssl_opts.keyStore = NULL;
  ssl_opts.trustStore = NULL;
  ssl_opts.privateKey = NULL;
  ssl_opts.privateKeyPassword = NULL;
  ssl_opts.enabledCipherSuites = NULL;
  conn_opts.ssl = &ssl_opts;
  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;

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

  // set username/password
  conn_opts.username = username;
  conn_opts.password = password;

  rc = MQTTClient_connect(client, &conn_opts);
  if ( MQTTCLIENT_SUCCESS != rc)
  {
    LOG(LOG_ERR, "MQTTClient_connect() returned %d", rc);
    goto cleanup;
  }

  try_reconnect = false;

  // clear out username and password in memory
  memset(username, 0, user_pass_len); // write over memory
  free(username); username = NULL;
  memset(password, 0, user_pass_len); // write over memory
  free(password); password = NULL;
  
  // deamonize only after we were able to get connected
  if (daemonize_flag) 
  {
    rc = daemonize_proc();
    if (rc != 0)
    {
      LOG(LOG_ERR, "daemonize_proc rc=%d", rc);
      goto cleanup;
    }
  }

  int bytes_read = 0;
  int rx_msg_len = 256;
  char* rx_msg = malloc(rx_msg_len*sizeof(char));
  if (!rx_msg)
  {
    LOG(LOG_ERR, "malloc rx_msg fail, %d", errno);
    goto cleanup;
  }

  MQTTClient_message pubmsg = MQTTClient_message_initializer;    

  while(!global_abort) 
  {

    if(try_reconnect)
    {
      rc = MQTTClient_connect(client, &conn_opts);
      if ( MQTTCLIENT_SUCCESS != rc)
      {
        LOG(LOG_WARN, "reconnection attempt, MQTTClient_connect() returned %d", rc);
        const struct timespec sleeptime = {5, 0};
        nanosleep(&sleeptime, NULL);
        continue;
      } 
      else 
      {
        LOG(LOG_INFO, "reconnection success");
        try_reconnect = false;
      }
    }

    memset(&rx_msg[0], 0, bytes_read);
    bytes_read = serial_read(rx_msg, rx_msg_len);
    if (bytes_read < 0)
    {
      LOG(LOG_ERR, "serial_read returned %d", bytes_read);
      global_abort = true;
    }
    else if (bytes_read != 0)
    {
      LOG(LOG_INFO, "publishing message %s to mqttify/device-rx", rx_msg);
      // publish the message
      pubmsg.payload = rx_msg;
      pubmsg.payloadlen = bytes_read;
      pubmsg.qos = QOS;
      pubmsg.retained = 0;
      rc = MQTTClient_publishMessage(client, "mqttify/device-rx", &pubmsg, NULL);
      if (MQTTCLIENT_SUCCESS != rc)
      {
        LOG(LOG_WARN, "publish failed rc=%d", rc);
      }

    }

  } // while(!global_abort)
  
cleanup:
  serial_cleanup();
  cleanup_and_exit();
  free(rx_msg);

  return rc;
}

int message_cb(void *context, char* topicName, int topicLen, MQTTClient_message *message)
{
  LOG(LOG_INFO, "message on %s : %s", topicName, (char*) message->payload);

  MQTTClient_freeMessage(&message);
  MQTTClient_free(&topicName);
  return 1;
}

void disconnect_cb(void *context, char* cause)
{
  LOG(LOG_WARN, "disconnect due to %s, trying to reconnect", cause);
  try_reconnect = true;
}

void cleanup_and_exit()
{
  MQTTClient_unsubscribe(client, "mqttify/device-tx");
  MQTTClient_disconnect(client, 100); // disconnects with a timeout of 100 
  MQTTClient_destroy(&client);
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
