#ifndef SARA_N200_ADAPTER_H
#define SARA_N200_ADAPTER_H

#include <Arduino.h>
#include <Stream.h>
#include "SaraN200AT.h"

#if defined(ESP32)
#undef max
#undef min
#include <functional>
#endif

typedef struct NameValuePair {
    const char* Name;
    const char* Value;
} NameValuePair;

class SaraN200 : public SaraN200AT {
public:
    SaraN200();
    virtual ~SaraN200() {}

    typedef std::function<ResponseType(ResponseType&, const char*, size_t, void*, void*)> CallbackMethodPtr;

    void init(Stream* stream);
    bool setRadioActive(bool on);
    bool isAlive();
    uint32_t getDefaultBaudRate() { return 9600; }
    bool createContext(const char* apn);
    bool connect(const char* apn);
    bool disconnect();
    bool isConnected();
    bool getRSSIAndBER(int8_t* rssi, uint8_t* ber);
    int8_t convertCSQ2RSSI(uint8_t csq) const;
    uint8_t convertRSSI2CSQ(int8_t rssi) const;

    int createSocket(uint8_t localPort = 42000, bool enableURC = false);
    int socketSendTo(int socket, IPAddress ip, uint8_t port, uint8_t* buffer, size_t size);
    int socketRecvFrom(int socket, uint8_t* buffer, size_t size);
    bool closeSocket(int socket);

protected:
    ResponseType readResponse(char* buffer, size_t size, size_t* outSize, uint32_t timeout = 5000) {
        return readResponse(inputBuffer, inputBufferSize, NULL, NULL, NULL, outSize, timeout);
    };

    ResponseType readResponse(char* buffer, size_t size,
                               CallbackMethodPtr parserMethod, void* callbackParameter, void* callbackParameter2 = NULL,
                               size_t* outSize = NULL, uint32_t timeout = 5000);

    ResponseType readResponse(size_t* outSize = NULL, uint32_t timeout = 5000) {
        return readResponse(inputBuffer, inputBufferSize, NULL, NULL, NULL, outSize, timeout);
    };

    ResponseType readResponse(CallbackMethodPtr parserMethod, void* callbackParameter,
                               void* callbackParameter2 = NULL, size_t* outSize = NULL, uint32_t timeout = 5000) {
        return readResponse(inputBuffer, inputBufferSize,
                            parserMethod, callbackParameter, callbackParameter2,
                            outSize, timeout);
    };

    template<typename T1, typename T2>
    ResponseType readResponse(CallbackMethodPtr parserMethod,
                               T1* callbackParameter, T2* callbackParameter2,
                               size_t* outSize = NULL, uint32_t timeout = 5000)
    {
        return readResponse(inputBuffer, inputBufferSize, parserMethod,
                            (void*)callbackParameter, (void*)callbackParameter2, outSize, timeout);
    };

private:
    static bool startsWith(const char* pre, const char* str);
    bool waitForSignalQuality(uint32_t timeout = 30 * 1000);
    bool attachGprs(uint32_t timeout = 30 * 1000);
    bool setConfigParam(const char* param, const char* value);
    bool checkAndApplyNconfig();
    void reboot();
};

#endif