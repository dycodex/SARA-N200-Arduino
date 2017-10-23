#include "SaraN200.h"

#define debugPrintln(...) { if (this->debugEnabled && this->debugStream) this->debugStream->println(__VA_ARGS__); }
#define debugPrint(...)  { if (this->debugEnabled && this->debugStream) this->debugStream->print(__VA_ARGS__); }

#define STR_AT "AT"
#define STR_RESPONSE_OK "OK"
#define STR_RESPONSE_ERROR "ERROR"
#define STR_RESPONSE_CME_ERROR "+CME ERROR:"
#define STR_RESPONSE_CMS_ERROR "+CMS ERROR:"

#define DEBUG_STR_ERROR "[ERROR]: "

#define NIBBLE_TO_HEX_CHAR(i) ((i <= 9) ? ('0' + i) : ('A' - 10 + i))
#define HIGH_NIBBLE(i) ((i >> 4) & 0x0F)
#define LOW_NIBBLE(i) (i & 0x0F)

#define HEX_CHAR_TO_NIBBLE(c) ((c >= 'A') ? (c - 'A' + 0x0A) : (c - '0'))
#define HEX_PAIR_TO_BYTE(h, l) ((HEX_CHAR_TO_NIBBLE(h) << 4) + HEX_CHAR_TO_NIBBLE(l))

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

#define DEFAULT_CID "0"

#define SOCKET_FAIL -1

#define SOCKET_COUNT 7

#define NOW (uint32_t)millis()

static const int nConfigCount = 3;
static NameValuePair nConfig[nConfigCount] = {
    {"AUTOCONNECT", "TRUE"},
    {"CR_0354_0338_SCRAMBLING", "TRUE"},
    {"CR_0859_SI_AVOID", "TRUE"}
};

static inline bool is_timedout(uint32_t from, uint32_t nr_ms) __attribute__((always_inline));
static inline bool is_timedout(uint32_t from, uint32_t nr_ms)
{
    return (millis() - from) > nr_ms;
}

SaraN200::SaraN200(): SaraN200AT() {
}

uint32_t SaraN200::getDefaultBaudrate() {
    return 9600;
}

bool SaraN200::isAlive() {
    println(STR_AT);

    return readResponse(NULL, 450) == ResponseOK;
}

void SaraN200::init(Stream* stream) {
    debugPrintln("[init] started");
    initBuffer();
    setModemStream(stream);
}

bool SaraN200::setRadioActive(bool on) {
    print("AT+CFUN=");
    println(on ? "1" : "0");

    return readResponse() == ResponseOK;
}

ResponseType SaraN200::readResponse(char* buffer, size_t size, CallbackMethodPtr parserMethod, void* callbackParameter, void* callbackParameter2, size_t* outSize, uint32_t timeout) {
    ResponseType response = ResponseNotFound;
    uint32_t from = NOW;

    do {
        int count = readln(buffer, size, 250);

        if (count > 0) {
            if (outSize) {
                *outSize = count;
            }

            if (!debugEnabled && strncmp(buffer, "OK", 2) != 0) {
                debugEnabled = true;
            }

            debugPrint("[read response]: ");
            debugPrintln(buffer);

            if (startsWith(STR_AT, buffer)) {
                continue;
            }

            debugEnabled = true;

            if (startsWith(STR_RESPONSE_OK, buffer)) {
                return ResponseOK;
            }

            if (startsWith(STR_RESPONSE_ERROR, buffer) || startsWith(STR_RESPONSE_CME_ERROR, buffer) || startsWith(STR_RESPONSE_CMS_ERROR, buffer)) {
                return ResponseError;
            }

            if (parserMethod) {
                ResponseType parserResponse = parserMethod(response, buffer, count, callbackParameter, callbackParameter2);
                if ((parserResponse != ResponseEmpty) && (parserResponse != ResponsePendingExtra)) {
                    return parserResponse;
                }

                if (parserResponse != ResponsePendingExtra) {
                    parserMethod = 0;
                }
            }

            if (response != ResponseNotFound) {
                debugPrintln("**Response != ResponseNotFound**");

                return response;
            }
        }

        delay(10);
    } while(!is_timedout(from, timeout));

    if (outSize) {
        *outSize = 0;
    }

    debugPrintln("[read response]: timed out");
    return ResponseTimeout;
}

bool SaraN200::createContext(const char* apn) {
    print("AT+CGDCONT=" DEFAULT_CID ",\"IP\",\"");
    print(apn);
    println("\"");

    return readResponse() == ResponseOK;
}

bool SaraN200::isConnected() {
    uint8_t value = 0;
    println("AT+CGATT?");

    if (readResponse<uint8_t, uint8_t>(cgAttParser, &value, NULL) == ResponseOK) {
        return value == 1;
    }

    return false;
}

ResponseType SaraN200::cgAttParser(ResponseType& response, const char* buffer, size_t size, uint8_t* result, uint8_t* unused) {
    if (!result) {
        return ResponseError;
    }

    int val;
    if (sscanf(buffer, "+CGATT: %d", &val) == 1) {
        *result = val;
        return ResponseEmpty;
    }

    return ResponseError;
}

bool SaraN200::disconnect() {
    println("AT+CGATT=0");

    return readResponse(NULL, 40 * 1000) == ResponseOK;
}

bool SaraN200::autoconnect() {
    if (!on()) {
        return false;
    }

    reboot();

    if (!waitForSignalQuality(30 * 1000)) {
        return false;
    }

    return waitForGprs(30 * 1000);
}

bool SaraN200::connect(const char* apn) {
    if (!on()) {
        return false;
    }

    if (!setRadioActive(false)) {
        return false;
    }

    if (!checkAndApplyNconfig()) {
        return false;
    }

    reboot();

    if (!on()) {
        return false;
    }

    if (isConnected()) {
        return true;
    }

    if (!setRadioActive(true)) {
        return false;
    }

    if (!attachGprs()) {
        return false;
    }

    return true;
}

bool SaraN200::getRSSIAndBER(int8_t* rssi, uint8_t* ber) {
    static char berValues[] = { 49, 43, 37, 25, 19, 13, 7, 0 };
    int csqRaw = 0;
    int berRaw = 0;

    println("AT+CSQ");

    if (readResponse<int, int>(csqParser, &csqRaw, &berRaw) == ResponseOK) {
        *rssi = (csqRaw == 99) ? 0 : convertCSQ2RSSI(csqRaw);
        *ber = (berRaw == 99 || static_cast<size_t>(berRaw) >= sizeof(berValues)) ? 0 : berValues[berRaw];

        return true;
    }

    return false;
}

ResponseType SaraN200::csqParser(ResponseType& response, const char* buffer, size_t size, int* csqResult, int* berResult) {
    if (!csqResult || !berResult) {
        return ResponseError;
    }

    if (sscanf(buffer, "+CSQ: %d,%d", csqResult, berResult) == 2) {
        return ResponseEmpty;
    }

    return ResponseError;
}

int8_t SaraN200::convertCSQ2RSSI(uint8_t csq) const {
    return -113 + 2 * csq;
}

uint8_t SaraN200::convertRSSI2CSQ(int8_t rssi) const {
    return (rssi + 113) / 2;
}

int SaraN200::createSocket(uint16_t localPort, bool enableURC) {
    print("AT+NSOCR=DGRAM,17,");
    print(localPort);
    print(",");
    println(enableURC ? "1" : "0");

    int fd = -1;

    if (readResponse<int, int>(createSocketParser, &fd, NULL) == ResponseOK) {
        return fd;
    }

    return -1;
}

ResponseType SaraN200::createSocketParser(ResponseType& response, const char* buffer, size_t size, int* socketFd, int* unused) {
    if (!socketFd) {
        return ResponseError;
    }

    if (sscanf(buffer, "%d\r", socketFd) == 1) {
        return ResponseEmpty;
    }

    return ResponseError;
}

int SaraN200::socketSendTo(int socket, IPAddress ip, uint16_t port, uint8_t* buffer, size_t size) {
    print("AT+NSOST=");
    print(socket);
    print(",");
    print(ip.toString());
    print(",");
    print(port);
    print(",");
    print(size);
    print(",");

    for (size_t i = 0; i < size; i++) {
        print(static_cast<char>(NIBBLE_TO_HEX_CHAR(HIGH_NIBBLE(buffer[i]))));
        print(static_cast<char>(NIBBLE_TO_HEX_CHAR(LOW_NIBBLE(buffer[i]))));
    }

    println();

    int usedSocket = -1;
    int sendLength = -1;

    if (readResponse<int, int>(socketSendToParser, &usedSocket, &sendLength) == ResponseOK) {
        return sendLength;
    }

    return -1;
}

ResponseType SaraN200::socketSendToParser(ResponseType& response, const char* buffer, size_t size, int* socketFd, int* length) {
    if (!socketFd || !length) {
        return ResponseError;
    }

    if (sscanf(buffer, "%d,%d", socketFd, length) == 2) {
        return ResponseEmpty;
    }

    return ResponseError;
}

int SaraN200::socketRecvFrom(int socket, uint8_t* buffer, size_t size) {
    print("AT+NSORF=");
    print(socket);
    print(",");
    println(size * 2);

    UdpDownlinkMesssage downlink;
    bool gotMessage = 0;
    if (readResponse<UdpDownlinkMesssage, bool>(socketRecvFromParser, &downlink, &gotMessage) == ResponseOK) {
        if (!gotMessage) {
            return -1;
        }

        for (int i = 0; i < downlink.dataLength * 2; i += 2) {
            char h = downlink.data[i];
            char l = downlink.data[i + 1];

            char convertedByte = HEX_PAIR_TO_BYTE(h, l);
            *buffer++ = static_cast<uint8_t>(convertedByte);
        }

        return downlink.dataLength;
    }

    return -1;
}

ResponseType SaraN200::socketRecvFromParser(ResponseType& response, const char* buffer, size_t size, UdpDownlinkMesssage* result, bool* gotResponse) {
    if (!result) {
        return ResponseError;
    }

    if (sscanf(buffer, "%d,%[0-9.],%d,%d,%[0-9a-fA-F],%d", &result->socket, result->fromIp, &result->fromPort, &result->dataLength, result->data, &result->remaining) == 6) {
        *gotResponse = true;
        return ResponseEmpty;
    }

    *gotResponse = false;
    return ResponseError;
}

bool SaraN200::closeSocket(int socket) {
    print("AT+NSOCL=");
    println(socket);

    return readResponse() == ResponseOK;
}

bool SaraN200::waitForSignalQuality(uint32_t timeout) {
    uint32_t start = millis();
    int8_t rssi;
    uint8_t ber;

    uint32_t interval = 2000;

    while (!is_timedout(start, timeout)) {
        if (millis() % interval == 0) {
            if (getRSSIAndBER(&rssi, &ber)) {
                if (rssi != 0) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool SaraN200::attachGprs(uint32_t timeout) {
    uint32_t start = millis();
    uint32_t delayCount = 500;

    while (!is_timedout(start, timeout)) {
        println("AT+CGATT=1");

        if (readResponse() == ResponseOK) {
            return true;
        }

        if (delayCount < 5000) {
            delayCount += 1000;
        }
    }

    return false;
}

bool SaraN200::setConfigParam(const char* param, const char* value) {
    print("AT+NCONFIG=");
    print(param);
    print(",");
    println(value);

    return readResponse() == ResponseOK;
}

bool SaraN200::checkAndApplyNconfig() {
    bool applyParamResult[nConfigCount];

    println("AT+NCONFIG?");

    if (readResponse<bool, uint8_t>(checkAndApplyNconfigParser, applyParamResult, NULL) == ResponseOK) {
        for (uint8_t i = 0; i < nConfigCount; i++) {
            debugPrint(nConfig[i].Name);
            if (!applyParamResult[i]) {
                debugPrintln("... CHANGE");
                setConfigParam(nConfig[i].Name, nConfig[i].Value);
            } else {
                debugPrintln("... OK");
            }
        }

        return true;
    }

    return false;
}

ResponseType SaraN200::checkAndApplyNconfigParser(ResponseType& response, const char* buffer, size_t size, bool* result, uint8_t* unused) {
    if (!result) {
        return ResponseError;
    }

    char name[32] = {0};
    char value[32] = {0};

    if (sscanf(buffer, "+NCONFIG: %[^,],%[^\r]", name, value) == 2) {
        for (uint8_t i = 0; i < nConfigCount; i++) {
            if (strcmp(nConfig[i].Name, name) == 0) {
                if (strcmp(nConfig[i].Value, value) == 0) {
                    result[i] = true;

                    break;
                }
            }
        }

        return ResponsePendingExtra;
    }

    return ResponseError;
}

void SaraN200::reboot() {
    println("AT+NRB");

    uint32_t start = millis();
    while ((readResponse() != ResponseOK) && !is_timedout(start, 2000)) {
    }
}

bool SaraN200::startsWith(const char* pre, const char* str) {
    return (strncmp(pre, str, strlen(pre)) == 0);
}

bool SaraN200::waitForGprs(uint32_t timeout) {
    uint32_t start = millis();
    uint32_t interval = 2000;

    while (!is_timedout(start, timeout)) {
        if (millis() % interval == 0) {
            if (isConnected()) {
                return true;
            }
        }
    }

    return false;
}
