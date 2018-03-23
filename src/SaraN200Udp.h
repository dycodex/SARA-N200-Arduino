#ifndef SARA_N200_UDP_INTERFACE_H
#define SARA_N200_UDP_INTERFACE_H

#include <Arduino.h>
#include <Udp.h>
#include <cbuf.h>
#include "SaraN200.h"

class SaraUDP: public UDP {
public:
    SaraUDP(SaraN200& sara);
    ~SaraUDP();

    virtual uint8_t begin(uint16_t port);
    virtual void stop();
    virtual int beginPacket();
    virtual int beginPacket(IPAddress ip, uint16_t port);
    virtual int beginPacket(const char* host, uint16_t port);
    virtual int endPacket();
    virtual size_t write(uint8_t value);
    virtual size_t write(const uint8_t* buffer, size_t size);
    virtual int parsePacket();
    virtual int available();
    virtual int read();
    virtual int read(unsigned char* buffer, size_t len);
    virtual int read(char* buffer, size_t len);
    virtual int peek();
    virtual void flush();
    virtual IPAddress remoteIP();
    virtual uint16_t remotePort();

private:
    SaraN200* sara_;
    int socket_;
    IPAddress rmtIp_;
    uint16_t rmtPort_;
    char* tx_buffer_;
    size_t tx_buffer_len_;
    cbuf* rx_buffer_;
};

#endif
