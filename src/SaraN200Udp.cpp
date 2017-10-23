#include "SaraN200Udp.h"

SaraUDP::SaraUDP(SaraN200& sara):
 sara(&sara),
 socket(-1),
 rmtPort(0),
 tx_buffer(0),
 tx_buffer_len(0),
 rx_buffer(0)
 {}

SaraUDP::~SaraUDP() {
    stop();
}

uint8_t SaraUDP::begin(uint16_t port) {
    // always return 0 since server is not supported
    return 0;
}

void SaraUDP::stop() {
    if (tx_buffer) {
        delete tx_buffer;
        tx_buffer = NULL;
    }

    if (rx_buffer) {
        delete rx_buffer;
        rx_buffer = NULL;
    }

    if (sara->closeSocket(socket)) {
        socket = -1;
    }
}

int SaraUDP::beginPacket() {
    if (rmtPort == 0) {
        return 0;
    }

    if (!tx_buffer) {
        tx_buffer = new char[512];
        if (!tx_buffer) {
            // log_e("could not create tx buffer: %d", errno);
            return 0;
        }
    }

    tx_buffer_len = 0;
    if (socket == -1) {
        socket = sara->createSocket();
        if (socket == -1) {
            return 0;
        }
    }

    return 1;
}

int SaraUDP::beginPacket(IPAddress ip, uint16_t port) {
    this->rmtPort = port;
    this->rmtIp = ip;

    return beginPacket();
}

int SaraUDP::beginPacket(const char* host, uint16_t port) {
    // TODO: handle this kind of thing
    return 0;
}

int SaraUDP::endPacket() {
    int sent = sara->socketSendTo(socket, rmtIp, rmtPort, (uint8_t*)tx_buffer, tx_buffer_len);

    if (sent == -1) {
        return 0;
    }

    return 1;
}

size_t SaraUDP::write(uint8_t value) {
    if (tx_buffer_len == 512) {
        endPacket();
        tx_buffer_len = 0;
    }

    tx_buffer[tx_buffer_len++] = value;
    return 1;
}

size_t SaraUDP::write(const uint8_t* buffer, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        write(buffer[i]);
    }
    return i;
}

int SaraUDP::parsePacket() {
    if (rx_buffer) {
        return 0;
    }

    uint8_t *buffer = new uint8_t[512];
    if (!buffer) {
        return 0;
    }

    int readLength = sara->socketRecvFrom(socket, buffer, 512);
    if (readLength == -1) {
        delete buffer;
        return 0;
    }

    rx_buffer = new cbuf(readLength);
    rx_buffer->write((const char*)buffer, readLength);

    return readLength;
}

int SaraUDP::available(){
    if(!rx_buffer) return 0;
    return rx_buffer->available();
}

int SaraUDP::read(){
    if(!rx_buffer) return -1;
    int out = rx_buffer->read();

    if(!rx_buffer->available()){
        cbuf *b = rx_buffer;
        rx_buffer = 0;
        delete b;
    }

    return out;
}

int SaraUDP::read(unsigned char* buffer, size_t len) {
    return read((char*)buffer, len);
}

int SaraUDP::read(char* buffer, size_t len) {
    if (!rx_buffer) {
        return 0;
    }

    int out = rx_buffer->read(buffer, len);
    if (!rx_buffer->available()) {
        cbuf *b = rx_buffer;
        rx_buffer = 0;
        delete b;
    }

    return out;
}

int SaraUDP::peek() {
    if (!rx_buffer) return -1;
    return rx_buffer->peek();
}

void SaraUDP::flush() {
    cbuf* b = rx_buffer;
    rx_buffer = 0;
    delete b;
}

IPAddress SaraUDP::remoteIP() {
    return rmtIp;
}

uint16_t SaraUDP::remotePort() {
    return rmtPort;
}
