#include "SaraN200AT.h"

#define debugPrintln(...)  { if (this->debugEnabled && this->debugStream) this->debugStream->println(__VA_ARGS__); }
#define debugPrint(...)  { if (this->debugEnabled && this->debugStream) this->debugStream->print(__VA_ARGS__); }

#define CR "\r"
#define LF "\n"
#define CRLF "\r\n"

#define SARA_AT_DEVICE_TERMINATOR CRLF

#ifndef SARA_AT_DEVICE_TERMINATOR
#warning "SARA_AT_DEVICE_TERMINATOR is not set"
#define SARA_AT_DEVICE_TERMINATOR CRLF
#endif

#define SARA_AT_DEVICE_TERMINATOR_LEN (sizeof(SARA_AT_DEVICE_TERMINATOR) - 1) // without the NULL terminator

SaraN200AT::SaraN200AT():
 debugStream(NULL),
 debugEnabled(false),
 inputBufferSize(250),
 isInputBufferInitialized(false),
 inputBuffer(0) {}

void SaraN200AT::setDebugStream(Stream* debug) {
    this->debugStream = debug;
}

void SaraN200AT::setDebugEnabled(bool state) {
    this->debugEnabled = state;
}

bool SaraN200AT::on() {
    startOn = millis();

    // TODO: actually turn on the module.

    bool timeout = false;
    for (uint8_t i = 0; i < 10; i++) {
        if (isAlive()) {
            timeout = false;
            break;
        }
    }

    if (timeout) {
        return false;
    }

    return isOn();
}

bool SaraN200AT::isOn() const {
    // TODO: actually check whether the module is on or off
    return true;
}

bool SaraN200AT::off() {
    // TODO: actually implement off function
    return !isOn();
}

void SaraN200AT::setInputBufferSize(size_t value) {
    this->inputBufferSize = value;
}

void SaraN200AT::setModemStream(Stream& stream) {
    this->modemStream = &stream;
}

void SaraN200AT::setModemStream(Stream* stream) {
    this->modemStream = stream;
}

void SaraN200AT::initBuffer() {
    if (!isInputBufferInitialized) {
        this->inputBuffer = static_cast<char*>(malloc(this->inputBufferSize));
        this->isInputBufferInitialized = true;
    }
}

int SaraN200AT::timedRead(uint32_t timeout) const {
    int c;
    uint32_t startTime = millis();

    do {
        c = modemStream->read();
        if (c > 0) {
            return c;
        }
    } while (millis() - startTime < timeout);

    return -1;
}

size_t SaraN200AT::readBytesUntil(char terminator, char* buffer, size_t length, uint32_t timeout) {
    if (length < 1) {
        return 0;
    }

    size_t index = 0;
    while (index < length) {
        int c = timedRead(timeout);

        if (c < 0 || c == terminator) {
            break;
        }

        *buffer++ = static_cast<char>(c);
        index++;
    }

    if (index < length) {
        *buffer = '\0';
    }

    return index;
}

size_t SaraN200AT::readBytes(uint8_t* buffer, size_t length, uint32_t timeout) {
    size_t count = 0;

    while (count < length) {
        int c = timedRead();
        if (c < 0) {
            break;
        }

        *buffer++ = static_cast<uint8_t>(c);
        count++;
    }

    return count;
}

size_t SaraN200AT::readln(char* buffer, size_t size, uint32_t timeout) {
    size_t len = readBytesUntil(SARA_AT_DEVICE_TERMINATOR[SARA_AT_DEVICE_TERMINATOR_LEN - 1], buffer, size - 1, timeout);

    if (buffer[len - 1] == '\r') {
        len -= 1;
    }

    if ((SARA_AT_DEVICE_TERMINATOR_LEN > 1) && (buffer[len - (SARA_AT_DEVICE_TERMINATOR_LEN - 1)] == SARA_AT_DEVICE_TERMINATOR[0])) {
        len -= SARA_AT_DEVICE_TERMINATOR_LEN - 1;
    }

    buffer[len] = '\0';

    return len;
}

size_t SaraN200AT::readln() {
    return readln(inputBuffer, inputBufferSize);
}

void SaraN200AT::writeProlog() {
    if (!appendCommand) {
        debugPrint(">> ");
        appendCommand = true;
    }
}

size_t SaraN200AT::writeByte(uint8_t value) {
    return modemStream->write(value);
}

size_t SaraN200AT::print(const __FlashStringHelper* fsh) {
    writeProlog();
    debugPrint(fsh);
    return modemStream->print(fsh);
}

size_t SaraN200AT::print(const String& buffer) {
    writeProlog();
    debugPrint(buffer);
    return modemStream->print(buffer);
}

size_t SaraN200AT::print(const char* buffer) {
    writeProlog();
    debugPrint(buffer);
    return modemStream->print(buffer);
}

size_t SaraN200AT::print(char c) {
    writeProlog();
    debugPrint(c);
    return modemStream->print(c);
}

size_t SaraN200AT::print(unsigned char uc, int base) {
    writeProlog();
    debugPrint(uc, base);
    return modemStream->print(uc, base);
}

size_t SaraN200AT::print(int i, int base) {
    writeProlog();
    debugPrint(i, base);
    return modemStream->print(i, base);
}

size_t SaraN200AT::print(unsigned int ui, int base) {
    writeProlog();
    debugPrint(ui, base);
    return modemStream->print(ui, base);
}

size_t SaraN200AT::print(long l, int base) {
    writeProlog();
    debugPrint(l, base);
    return modemStream->print(l, base);
}


size_t SaraN200AT::print(unsigned long ul, int base) {
    writeProlog();
    debugPrint(ul, base);
    return modemStream->print(ul, base);
}

size_t SaraN200AT::print(double d, int base) {
    writeProlog();
    debugPrint(d, base);
    return modemStream->print(d, base);
}

size_t SaraN200AT::print(const Printable& printable) {
    writeProlog();
    debugPrint(printable);
    return modemStream->print(printable);
}

size_t SaraN200AT::println(const __FlashStringHelper* ifsh) {
    debugPrintln("");
    size_t n = print(ifsh);
    n += println();
    return n;
}

size_t SaraN200AT::println(const String& s) {
    size_t n = print(s);
    n += println();
    return n;
}


size_t SaraN200AT::println(const char* c) {
    size_t n = print(c);
    n += println();
    return n;
}

size_t SaraN200AT::println(char c) {
    size_t n = print(c);
    n += println();
    return n;
}

size_t SaraN200AT::println(unsigned char b, int base) {
    size_t i = print(b, base);
    return i + println();
}

size_t SaraN200AT::println(int num, int base) {
    size_t i = print(num, base);
    return i + println();
}

size_t SaraN200AT::println(unsigned int num, int base) {
    size_t i = print(num, base);
    return i + println();
}

size_t SaraN200AT::println(long num, int base) {
    size_t i = print(num, base);
    return i + println();
}

size_t SaraN200AT::println(unsigned long num, int base) {
    size_t i = print(num, base);
    return i + println();
}

size_t SaraN200AT::println(double num, int digits) {
    writeProlog();
    debugPrint(num, digits);

    return modemStream->println(num, digits);
}

size_t SaraN200AT::println(const Printable& x) {
    size_t i = print(x);
    return i + println();
}

size_t SaraN200AT::println(void) {
    debugPrintln("");
    size_t i = print('\r');
    appendCommand = false;
    return i;
}
