#include <mbed.h>
#include "EthernetInterface.h"
#include "trackle.h"
#include "trackle_hardcoded_credentials.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

EthernetInterface net;
SocketAddress sockAddr;
UDPSocket sock;
Trackle *trackle_s;
Thread thread(osPriorityNormal, 16 * 1024);

// Cloud POST functions
static int funSuccess(const char *args);
static int funFailure(const char *args);
static int incrementCloudNumber(const char *args);

// Cloud GET functions
static const void *getCloudNumberMessage(const char *args);
static const void *getHalfCloudNumber(const char *args);

// Cloud GET variables
static int cloudNumber = 0;

static system_tick_t getMillis(void)
{
  return us_ticker_read() / 1000L;
}

void log_cb(const char *msg, int level, const char *category, void *attribute, void *reserved)
{
  printf("%d Log_cb: (%s) -> %s\n", getMillis(), (category ? category : ""), msg);
  return;
}

void sleep_cb(uint32_t sleep)
{
  ThisThread::sleep_for(sleep);
  return;
}

int connect_cb_udp(const char *address, int port)
{
  sock.open(&net);
  sock.set_blocking(true);
  sock.set_timeout(1);

  net.gethostbyname(address, &sockAddr);
  sockAddr.set_port(port);

  return 1;
}

int disconnect_cb()
{
  sock.close();
  return 1;
}

int send_cb_udp(const unsigned char *buf, uint32_t buflen, void *tmp)
{
  int sent = sock.sendto(sockAddr, buf, buflen);

  // size_t sent = sendto(cloud_socket, (const char *)buf, buflen, 0, (struct sockaddr *)&cloud_addr, sizeof(cloud_addr));
  if ((int)sent > 0)
  {
    printf("send_cb_udp sent %d\n", sent);
  }

  return (int)sent;
}

int receive_cb_udp(unsigned char *buf, uint32_t buflen, void *tmp)
{
  nsapi_size_or_error_t size = sock.recvfrom(NULL, buf, buflen);
  if (size <= 0)
  {
    if (size == NSAPI_ERROR_WOULD_BLOCK)
      return 0;
  }
  else
  {
    buf[size] = '\0';
    printf("Received %d bytes from UDP socket)\n", size);
  }

  return (int)size;
}

// BEGIN -- Cloud POST functions --------------------------------------------------------------------------------------------------------------------

static int funSuccess(const char *args)
{
  return 1;
}

static int funFailure(const char *args)
{
  return -1;
}

static int incrementCloudNumber(const char *args)
{
  cloudNumber++;
  return 1;
}

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- Cloud GET functions --------------------------------------------------------------------------------------------------------------------

static char cloudNumberMessage[64];
static const void *getCloudNumberMessage(const char *args)
{
  sprintf(cloudNumberMessage, "The number is %d !", cloudNumber);
  return cloudNumberMessage;
}

static const void *getHalfCloudNumber(const char *args)
{
  static char buffer[40];
  buffer[0] = '\0';
  sprintf(buffer, "{\"halfCloudNumber\":%d}", cloudNumber / 2);
  return buffer;
}

// END -- Cloud GET functions ----------------------------------------------------------------------------------------------------------------------

void trackle_thread()
{
  // trackle
  trackle_s = new Trackle();
  trackle_s->setDeviceId(HARDCODED_DEVICE_ID);

  // calback per i log e livello del log
  trackle_s->setLogCallback(log_cb);
  trackle_s->setLogLevel(TRACKLE_INFO);
  trackle_s->setConnectionType(CONNECTION_TYPE_WIFI);

  // configurazione delle callback
  trackle_s->setMillis(getMillis);
  trackle_s->setSendCallback(send_cb_udp);
  trackle_s->setReceiveCallback(receive_cb_udp);
  trackle_s->setConnectCallback(connect_cb_udp);
  trackle_s->setDisconnectCallback(disconnect_cb);
  trackle_s->setSleepCallback(sleep_cb);
  trackle_s->setKeys(HARDCODED_PRIVATE_KEY);

  // Registering POST functions callable from cloud
  trackle_s->post("funSuccess", (int (*)(const char *, ...))funSuccess, ALL_USERS);
  trackle_s->post("funFailure", (int (*)(const char *, ...))funFailure, ALL_USERS);
  trackle_s->post("incrementCloudNumber", (int (*)(const char *, ...))incrementCloudNumber, ALL_USERS);

  // Registering values GETtable from cloud as result of a function call
  trackle_s->get("getCloudNumberMessage", (void *(*)(const char *))getCloudNumberMessage, VAR_STRING);
  trackle_s->get("getHalfCloudNumber", (void *(*)(const char *))getHalfCloudNumber, VAR_JSON);

  int res = trackle_s->connect();

  while (1)
  {
    trackle_s->loop();
    ThisThread::sleep_for(10);
  }
}

int main()
{

  // Bring up the ethernet interface
  printf("Firmware starting...\n");
  if (0 != net.connect())
  {
    printf("Error connecting\n");
    return -1;
  }

  // Show the network address
  net.get_ip_address(&sockAddr);
  printf("IP address is: %s\n", sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "No IP");

  thread.start(trackle_thread);

  while (true)
  {
    ThisThread::sleep_for(500);
  }
}