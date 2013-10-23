/*************************************************** 
  select() and CC3000 read() speed tests
  
  Run generator.py on a machine first, then update the network
  and server IP/port info below to run.
  
  Written by Tony DiCola, based on code example code written by
  Limor Fried & Kevin Townsend for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/
 
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include "utility/socket.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIV2); // you can change this clock speed

#define WLAN_SSID       "myNetwork"           // cannot be longer than 32 characters!
#define WLAN_PASS       "myPassword"

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Test server configuration
const uint8_t   SERVER_IP[4]       = { 192, 168, 1, 101 };
const uint16_t  SERVER_PORT        = 9000;

#define         READ_ITERATIONS    1000 
// Note: Need to change some string constants in the test if read iterations is changed.

// Run the test
void runTest(void) {
  // Make a connection to the server.  
  // Manual socket use is done here so we can call select without having to hack
  // up the CC3000 library to expose the socket.
  int32_t client1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  sockaddr socketAddress;
  uint32_t destIP = cc3000.IP2U32(SERVER_IP[0], SERVER_IP[1], SERVER_IP[2], SERVER_IP[3]);
  uint16_t destPort = SERVER_PORT;
  memset(&socketAddress, 0x00, sizeof(socketAddress));
  socketAddress.sa_family = AF_INET;
  socketAddress.sa_data[0] = (destPort & 0xFF00) >> 8;  // Set the Port Number
  socketAddress.sa_data[1] = (destPort & 0x00FF);
  socketAddress.sa_data[2] = destIP >> 24;
  socketAddress.sa_data[3] = destIP >> 16;
  socketAddress.sa_data[4] = destIP >> 8;
  socketAddress.sa_data[5] = destIP;
  if (connect(client1, &socketAddress, sizeof(socketAddress)) == -1) {
    Serial.println(F("Failure to connect to the server!"));
    while(1); 
  }

  Serial.println(F("Connected!"));
  
  // Setup some state for calling select later
  unsigned long total; 
  uint8_t ch;
  timeval timeout;
  fd_set fd_read;
  timeout.tv_sec = 0;
  timeout.tv_usec = 5000; // 5 millisec
  
  // Time how long each select takes when bytes are queued
  Serial.println(F("Testing how long each select() call takes when bytes are available."));
  // Ask the server to give us 1000 bytes and wait a short while for the response.
  send(client1, "1000" "\n", sizeof("1000" "\n")-1, 0);
  delay(200);
  total = 0;
  for (int i = 0; i < READ_ITERATIONS; ++i) {
    // Prepare for select
    memset(&fd_read, 0, sizeof(fd_read));
    FD_SET(client1, &fd_read);
    // Time the select call
    unsigned long start = micros();
    uint16_t response = select(client1+1, &fd_read, NULL, NULL, &timeout);
    total += (micros() - start);
    // Check there was data to read
    if (!(response == 1 && FD_ISSET(client1, &fd_read))) {
      Serial.println(F("Expected select to return data available for reading!"));
    }    
    // Remove a character
    recv(client1, &ch, 1, 0); 
  }
  Serial.print(F("Execution of ")); Serial.print(READ_ITERATIONS); Serial.print(F(" calls took "));
  Serial.print(total);
  Serial.println(F(" microseconds."));
  
  // Time how long each select takes when one byte is available to read
  Serial.println(F("Testing how long each select() call takes when one byte is available to read."));
  total = 0;
  for (int i = 0; i < 1000; ++i) {
    // Ask the server to send a byte and wait a short period of time for the response.
    send(client1, "1\n", sizeof("1\n")-1, 0);
    delay(50);
    // Prepare for select
    memset(&fd_read, 0, sizeof(fd_read));
    FD_SET(client1, &fd_read);
    // Time the select call
    unsigned long start = micros();
    uint16_t response = select(client1+1, &fd_read, NULL, NULL, &timeout);
    total += (micros() - start);
    // Check data is available to read
    if (!(response == 1 && FD_ISSET(client1, &fd_read))) {
      Serial.println(F("Expected select to return data available for reading!"));
    }
    // Remove a character
    recv(client1, &ch, 1, 0); 
  }
  Serial.print(F("Execution of ")); Serial.print(READ_ITERATIONS); Serial.print(F(" calls took "));
  Serial.print(total);
  Serial.println(F(" microseconds."));
  
  // Time how long each select takes when no bytes are available to read
  Serial.println(F("Testing how long each select() call takes when no bytes are available to read."));
  total = 0;
  for (int i = 0; i < 1000; ++i) {
    // Prepare for select
    memset(&fd_read, 0, sizeof(fd_read));
    FD_SET(client1, &fd_read);
    // Time the select call
    unsigned long start = micros();
    uint16_t response = select(client1+1, &fd_read, NULL, NULL, &timeout);
    total += (micros() - start);
    // Check no data is available to read
    if (response == 1 || FD_ISSET(client1, &fd_read)) {
      Serial.println(F("Expected select to NOT return data available for reading!"));
    }
  }
  Serial.print(F("Execution of ")); Serial.print(READ_ITERATIONS); Serial.print(F(" calls took "));
  Serial.print(total);
  Serial.println(F(" microseconds."));
  
  closesocket(client1);
  
  // Now use the CC3000 library class to test how long it takes to call available() with data buffered
  Adafruit_CC3000_Client client2 = cc3000.connectTCP(cc3000.IP2U32(SERVER_IP[0], SERVER_IP[1], SERVER_IP[2], SERVER_IP[3]), 
                                                     SERVER_PORT);
  if (!client2.connected()) {
    Serial.println(F("Failure to connect to server!"));
    while(1);
  }
  
  // Time how long each available call takes when bytes are queued
  Serial.println(F("Testing how long each CC3000 library available() call takes when bytes are available."));
  // Ask the server to give us 1001 bytes and wait a short while for the response.
  // Note: The extra byte is so read() can be called to fill the rx queue.
  client2.fastrprintln("1001");
  delay(200);
  client2.read(); // Prime the queue by making a read that will fill it.
  total = 0;
  for (int i = 0; i < READ_ITERATIONS; ++i) {
    // Time the available call
    unsigned long start = micros();
    uint8_t response = client2.available();
    total += (micros() - start);
    // Check there was data to read
    if (response < 1) {
      Serial.println(F("Expected available to return data available for reading!"));
    }    
    // Remove a character
    client2.read();
  }
  Serial.print(F("Execution of ")); Serial.print(READ_ITERATIONS); Serial.print(F(" calls took "));
  Serial.print(total);
  Serial.println(F(" microseconds."));
  
  client2.close();
  
  cc3000.disconnect();
}

// Set up the HW and the CC3000 module (called automatically on startup)
void setup(void)
{
  Serial.begin(115200);
  Serial.println(F("Hello, CC3000!\n")); 
  
  /* Initialise the module */
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  /* Display the IP address DNS, Gateway, etc. */  
  while (! displayConnectionDetails()) {
    delay(1000);
  }
  
  runTest();
}

void loop(void)
{
 delay(1000);
}

// Tries to read the IP address and other connection details
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}
