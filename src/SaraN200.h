#ifndef SARA_N200_ADAPTER_H
#define SARA_N200_ADAPTER_H

#include <Arduino.h>
#include <Stream.h>
#include "SaraN200AT.h"

class SaraN200 : public SaraN200AT {
public:

    typedef struct NameValuePair {
        const char* Name;
        const char* Value;
    } NameValuePair;

    typedef struct UdpDownlinkMesssage {
        int socket;
        char fromIp[48];
        uint16_t fromPort;
        size_t dataLength;
        char data[512];
        size_t remaining;
    } UdpDownlinkMesssage;

    SaraN200();
    virtual ~SaraN200() {}

    typedef ResponseType(*CallbackMethodPtr)(ResponseType& response, const char* buffer, size_t size, void* param, void* param2);

    void init(Stream* stream);
    bool setRadioActive(bool on);
    bool isAlive();
    virtual uint32_t getDefaultBaudrate();
    bool autoconnect();
    bool createContext(const char* apn);
    bool connect(const char* apn, bool noAutoconnect = true);
    bool disconnect();
    bool isConnected();
    bool getRSSIAndBER(int8_t* rssi, uint8_t* ber);
    int8_t convertCSQ2RSSI(uint8_t csq) const;
    uint8_t convertRSSI2CSQ(int8_t rssi) const;

    int createSocket(uint16_t localPort = 42000, bool enableURC = false);
    int socketSendTo(int socket, IPAddress ip, uint16_t port, uint8_t* buffer, size_t size);
    int socketRecvFrom(int socket, uint8_t* buffer, size_t size);
    bool closeSocket(int socket);

    bool sleep();

    bool printThroughputInfo();
    bool printCellStatsInfo();

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
    ResponseType readResponse(ResponseType(*parserMethod)(ResponseType& response, const char* parseBuffer, size_t size, T1* parameter, T2* parameter2),
                               T1* callbackParameter, T2* callbackParameter2,
                               size_t* outSize = NULL, uint32_t timeout = 5000)
    {
        return readResponse(inputBuffer, inputBufferSize, (CallbackMethodPtr)parserMethod,
                            (void*)callbackParameter, (void*)callbackParameter2, outSize, timeout);
    };

private:
    static bool startsWith(const char* pre, const char* str);
    bool waitForSignalQuality(uint32_t timeout = 30 * 1000);
    bool waitForGprs(uint32_t timeout = 30 * 1000);
    bool attachGprs(uint32_t timeout = 30 * 1000);
    bool setConfigParam(const char* param, const char* value);
    bool checkAndApplyNconfig(bool forceNoAutoconnect = false);
    void reboot();

    static ResponseType cgAttParser(ResponseType& response, const char* buffer, size_t size, uint8_t* result, uint8_t* unused);
    static ResponseType csqParser(ResponseType& response, const char* buffer, size_t size, int* csqResult, int* berResult);
    static ResponseType createSocketParser(ResponseType& response, const char* buffer, size_t size, int* socketFd, int* unused);
    static ResponseType socketSendToParser(ResponseType& response, const char* buffer, size_t size, int* socketFd, int* length);
    static ResponseType checkAndApplyNconfigParser(ResponseType& response, const char* buffer, size_t size, bool* result, uint8_t* unused);
    static ResponseType socketRecvFromParser(ResponseType& response, const char* buffer, size_t size, UdpDownlinkMesssage* result, bool* indicator);
};

#endif
