#include "SaraN200Udp.h"

SaraUDP::SaraUDP(SaraN200& sara):
 sara_(&sara),
 socket_(-1),
 rmtPort_(0),
 tx_buffer_(0),
 tx_buffer_len_(0),
 rx_buffer_(0)
 {}

SaraUDP::~SaraUDP() {
    stop();
}

uint8_t SaraUDP::begin(uint16_t port) {
    // always return 0 since server is not supported
    return 0;
}

void SaraUDP::stop() {
    if (tx_buffer_) {
        delete tx_buffer_;
        tx_buffer_ = NULL;
    }

    if (rx_buffer_) {
        delete rx_buffer_;
        rx_buffer_ = NULL;
    }

    if (sara_->closeSocket(socket_)) {
        socket_ = -1;
    }
}

int SaraUDP::beginPacket() {
    if (rmtPort_ == 0) {
        return 0;
    }

    if (!tx_buffer_) {
        tx_buffer_ = new char[512];
        if (!tx_buffer_) {
            // log_e("could not create tx buffer: %d", errno);
            return 0;
        }
    }

    tx_buffer_len_ = 0;
    if (socket_ == -1) {
        socket_ = sara_->createSocket();
        if (socket_ == -1) {
            return 0;
        }
    }

    return 1;
}

int SaraUDP::beginPacket(IPAddress ip, uint16_t port) {
    rmtPort_ = port;
    rmtIp_ = ip;

    return beginPacket();
}

int SaraUDP::beginPacket(const char* host, uint16_t port) {
    // TODO: handle this kind of thing
    return 0;
}

int SaraUDP::endPacket() {
    int sent = sara_->socketSendTo(socket_, rmtIp_, rmtPort_, (uint8_t*)tx_buffer_, tx_buffer_len_);

    if (sent == -1) {
        return 0;
    }

    return 1;
}

size_t SaraUDP::write(uint8_t value) {
    if (tx_buffer_len_ == 512) {
        endPacket();
        tx_buffer_len_ = 0;
    }

    tx_buffer_[tx_buffer_len_++] = value;
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
    if (rx_buffer_) {
        return 0;
    }

    uint8_t buffer[512] = {0};

    int readLength = sara_->socketRecvFrom(socket_, buffer, 512);
    if (readLength == -1) {
        return 0;
    }

    rx_buffer_ = new cbuf(readLength);
    rx_buffer_->write((const char*)buffer, readLength);

    return readLength;
}

int SaraUDP::available(){
    if(!rx_buffer_) return 0;
    return rx_buffer_->available();
}

int SaraUDP::read(){
    if(!rx_buffer_) return -1;
    int out = rx_buffer_->read();

    if(!rx_buffer_->available()){
        cbuf *b = rx_buffer_;
        rx_buffer_ = 0;
        delete b;
    }

    return out;
}

int SaraUDP::read(unsigned char* buffer, size_t len) {
    return read((char*)buffer, len);
}

int SaraUDP::read(char* buffer, size_t len) {
    if (!rx_buffer_) {
        return 0;
    }

    int out = rx_buffer_->read(buffer, len);
    if (!rx_buffer_->available()) {
        cbuf *b = rx_buffer_;
        rx_buffer_ = 0;
        delete b;
    }

    return out;
}

int SaraUDP::peek() {
    if (!rx_buffer_) return -1;
    return rx_buffer_->peek();
}

void SaraUDP::flush() {
    cbuf* b = rx_buffer_;
    rx_buffer_ = 0;
    delete b;
}

IPAddress SaraUDP::remoteIP() {
    return rmtIp_;
}

uint16_t SaraUDP::remotePort() {
    return rmtPort_;
}
