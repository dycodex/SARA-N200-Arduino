#include "SaraN200.h"

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

#define DEFAULT_CID "1"

#define SOCKET_FAIL -1

#define SOCKET_COUNT 7

#define NOW (uint32_t)millis()

static const nConfigCount = 3;
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

bool SaraN200::isAlive() {
    debugEnabled = false;
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

ResponseType SaraN200::readResponse(char* buffer, size_t, CallbackMethodPtr parserMethod, void* callbackParameter, void* callbackParameter2, size_t* outSize, uint32_t timeout) {
    ResponSeType response = ResponseNotFound;
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

    debugPrintLn("[read response]: timed out");
    return ResponseTimeout;
}

bool SaraN200::createContet(const char* apn) {
    print("AT+CGDCONT=" DEFAULT_CID ",\"IP\",\"");
    print(apn);
    pritnln("\"");

    return readResponse() == ResponseOK;
}

bool SaraN200::isConnected() {
    uint8_t value = 0;
    println("AT+CGATT?");

    auto parser = [&](ResponseType response, const char* buffer, size_t size, uint8_t* result, uint8_t* unused) {
        if (!result) {
            return ResponseError;
        }

        int val;
        if (sscanf(buffer, "+CGATT: %d", &val) == 1) {
            *result = val;
            return ResponseEmpty;
        }

        return ResponseError;
    };

    if (readResponse<uint8_t, uint8_t>(parser, &value, NULL) == ResponseOK) {
        return value == 1;
    }

    return false;
}

bool SaraN200::disconnect() {
    println("AT+CGATT=0");

    return readResponse(NULL, 40 * 1000) == ResponseOK;
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

    if (!waitForSignalQuality()) {
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

    auto csqParser = [&](ResponseType response, const char* buffer, size_t size, int* csqResult, int* berResult) {
        if (!rssi || !ber) {
            return ResponseError;
        }

        if (sscanf(buffer, "+CSQ: %d,%d", rssi, ber) == 2) {
            return ResponseEmpty;
        }

        return ResponseError;
    };

    if (readResponse<int, int>(csqParser, &csqRaw, &berRaw) == ResponseOK) {
        *rssi = (csqRaw == 99) ? 0 : convertCSQ2RSSI(csqRaw);
        *ber = (berRaw == 99 || static_cast<size_t>(berRaw) >= sizeof(berValues)) ? 0 : berValues[berRaw];

        return true;
    }

    return false;
}

int8_t SaraN200::convertCSQ2RSSI(uint8_t csq) const {
    return -113 + 2 * csq;
}

uint8_t SaraN200::convertRSSI2CSQ(int8_t rssi) const {
    return (rssi + 113) / 2;
}

int SaraN200::createSocket(uint8_t localPort, bool enableURC) {
    print("AT+NSOCR=DGRAM,17,");
    print(localPort);
    print(",");
    println(enableURC ? "1" : "0");

    auto socketParser = [&](REsponseType response, const char* buffer, size_t size, int* socketFd, int* unused) {
        if (!socketFd) {
            return ResponseError;
        }

        if (sscanf(buffer, "%d", socketFd) == 1) {
            return ResponseEmpty;
        }

        return ResponseError;
    }

    int fd = -1;

    if (readResponse<int, int>(socketParser, &fd, NULL) == ResponseOK) {
        return fd;
    }

    return -1;
}

int SaraN200::socetSendTo(int socket, IPAddress ip, uint8_t port, uint8_t* buffer, size_t size) {
    print("AT+NSOST=");
    print(socket);
    print(",");
    print(ip.toString());
    print(",");
    print(port);
    print(",");
    print(size);

    for (size_t i = 0; i < size; i++) {
        print(static_cast<char>(NIBBLE_TO_HEX_CHAR(HIGH_NIBBLE(buffer[i]))));
        print(static_cast<char>(NIBBLE_TO_HEX_CHAR(LOW_NIBBLE(buffer[i]))));
    }

    println();

    int usedSocket = -1;
    int sendLength = -1;
    auto socketSendParser = [&](ResponseType response, char* buffer, size_t size, int* fd, int* length) {
        if (!fd || !length) {
            return ResponseError;
        }

        if (sscanf("%d,%d", fd, length) == 2) {
            return ResponseEmpty;
        }

        return ResponseError;
    };

    if (readResponse<int, int>(socketSendParser, &usedSocket, &sendLength) == ResponseOK) {
        return sendLength;
    }

    return -1;
}

int SaraN200::socketRecvFrom(int socket, uint8_t* buffer, size_t size) {
    print("AT+NSORF=");
    print(socket);
    print(",")
    println(size * 2);

    char* response = new char[556];
    size_t outSize = 0;

    if (readResponse(response, 556, NULL, NULL, NULL, &outSize, timeout) == ResponseOK) {
        int parsedSize = 0;
        int remainingSize = 0;
        int fromSocketFd = -1;
        char* fromIp = new char[48];
        char* retrievedData = new char[513];
        int fromPort = 0;

        if (sscanf("%d,%s,%d,%d,%s,%d", fromSocketFd, fromIp, fromPort, parsedSize, retrievedData, remainingSize) != 6) {
            delete response;
            delete fromIp;
            delete retrievedData;
            return -1;
        }

        debugPrint("[recv] Got data from: ");
        debugPrint(fromIp);
        debugPrint(". Size: ");
        debugPrintln(parsedSize);
        debugPrint("[recv] Data: ");
        debugPrintln(retrievedData);

        for (int i = 0; i < parsedSize * 2; i += 2) {
            char h = retrievedData[i];
            char l = retrievedData[i + 1];

            char convertedByte = HEX_PAIR_TO_BYTE(h, l);
            *buffer++ = static_cast<uint8_t>(convertedByte);
        }

        delete retrievedData;
        delete fromIp;
        delete response;

        return parsedSize;
    }

    delete response;

    return -1;
}

bool SaraN200::closeSocket(int socket) {
    println("AT+NSOCL=1");

    return readResponse() == ResponseOK;
}

bool SaraN200::waitForSignalQuality(uint32_t timeout) {
    uint32_t start = millis();
    int8_t rssi;
    uint8_t ber;

    uint32_t delayCount = 500;

    while (!is_timedout(start, timeout)) {
        if (getRSSIAndBER(&rssi, &ber)) {
            if (rssi != 0) {
                return true;
            }
        }

        if (delayCount < 5000) {
            delayCount += 1000;
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

    auto nconfigParser = [&](ResponseType response, char* buffer, size_t size, bool* result, uint8_t* unused) {
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
    };

    if (readResponse<bool, uint8_t>(nconfigParser, applyParamResult, NULL) == ResponseOK) {
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

void SaraN200::reboot() {
    println("AT+NRB");

    uint32_t start = millis();
    while ((readResponse() != ResponseOK) && !is_timedout(start, 2000)) {
    }
}

bool SaraN200::startsWIth(const char* pre, const char* str) {
    return (strncmp(pre, str, strlen(pre)) == 0);
}
