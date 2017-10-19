#ifndef SARA_N200_AT_DEVICE
#define SARA_N200_AT_DEVICE

#include <Arduino.h>
#include <stdint.h>
#include <Stream.h>

typedef enum {
    ResponseNotFound = 0,
    ResponseOK,
    ResponseError,
    ResponsePrompt,
    ResponseTimeout,
    ResponseEmpty,
    ResponsePendingExtra,
} ResponseType;

class SaraN200AT {
public:
    SaraN200AT();
    ~SaraN200AT();

    void setDebugStream(Stream* debug);
    void setDebugEnabled(bool state);
    bool on();
    bool off();
    void setInputBufferSize(size_t value);

    // implement this on the actual class
    virtual uint32_t getDefaultBaudrate() = 0;

protected:
    Stream* modemStream;
    Stream* debugStream;
    bool debugEnabled;

    size_t inputBufferSize;
    bool isInputBufferInitialized;
    char* inputBuffer;

    uint32_t startOn;
    bool appendCommand;

    void setModemStream(Stream& stream);
    void setModemStream(Stream* stream);

    // implement this on the actual class
    virtual bool isAlive() = 0;
    virtual ResponseType readResponse(char* buffer, size_t size, size_t* outSize, uint32_t timeout = 5000) = 0;

    bool isOn() const;
    void initBuffer();

    int timedRead(uint32_t timeout = 1000) const;
    size_t readBytesUntil(char terminator, char* buffer, size_t length, uint32_t timeout = 1000);
    size_t readBytes(uint8_t* buffer, size_t length, uint32_t timeout = 1000);
    size_t readln(char* buffer, size_t size, uint32_t timeout = 1000);
    size_t readln();

    void writeProlog();

    size_t writeByte(uint8_t value);
    size_t print(const __FlashStringHelper*);
    size_t print(const String&);
    size_t print(const char[]);
    size_t print(char);
    size_t print(unsigned char, int = DEC);
    size_t print(int, int = DEC);
    size_t print(unsigned int, int = DEC);
    size_t print(long, int = DEC);
    size_t print(unsigned long, int = DEC);
    size_t print(double, int = 2);
    size_t print(const Printable&);

    size_t println(const __FlashStringHelper*);
    size_t println(const String&);
    size_t println(const char[]);
    size_t println(char);
    size_t println(unsigned char, int = DEC);
    size_t println(int, int = DEC);
    size_t println(unsigned int, int = DEC);
    size_t println(long, int = DEC);
    size_t println(unsigned long, int = DEC);
    size_t println(double, int = 2);
    size_t println(const Printable&);
    size_t println(void);
};

#endif
