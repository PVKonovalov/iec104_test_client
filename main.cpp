/*
*  Copyright 2026 Pavel Konovalov
 *
 *  This file is part of iec104_test_client
 *
 *  iec104_test_client is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  iec104_test_client is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with iec104_test_client. If not, see <http://www.gnu.org/licenses/>.
 *
 *  See LICENSE file for the complete license text.
 */

#include <condition_variable>
#include <mutex>
#include <hal_thread.h>
#include <cs104_connection.h>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cppflags/cppflags.h>

#ifdef _WIN32
#include <signal.h>
#else
#include <sys/signal.h>
#endif
#include "src/Context/Context.h"

static constexpr auto VERSION = "1.0.0";

void working(const char *hostname, int tcpPort);

static std::mutex mtx;
static std::condition_variable cv;
static bool notified = false;
static bool running = true;
static bool reconnecting = true;

int filterIoa = 0;

std::atomic<CS104_ConnectionEvent> lastEvent = CS104_CONNECTION_CLOSED;

/** SIGINT handler: requests a graceful shutdown by cancelling the context and clearing the running/reconnecting flags */
void sigint_handler(int signalId) {
    context->Cancel();
    running = false;
    reconnecting = false;
}

sCS104_APCIParameters currentAPCIParameters = {
    /* .k = */ 12,
    /* .w = */ 8,
    /* .t0 = */ 30,
    /* .t1 = */ 15,
    /* .t2 = */ 10,
    /* .t3 = */ 20
};

static sCS101_AppLayerParameters currentAppLayerParameters = {
    /* .sizeOfTypeId =  */ 1,
    /* .sizeOfVSQ = */ 1,
    /* .sizeOfCOT = */ 2,
    /* .originatorAddress = */ 0,
    /* .sizeOfCA = */ 2,
    /* .sizeOfIOA = */ 3,
    /* .maxSizeOfASDU = */ 249
};

/** Converts a CP56Time2a timestamp to a broken-down struct tm (local calendar fields, no DST flag) */
tm CP56Time2a_toTm(const CP56Time2a self) {
    tm tmTime{};

    tmTime.tm_sec = CP56Time2a_getSecond(self);
    tmTime.tm_min = CP56Time2a_getMinute(self);
    tmTime.tm_hour = CP56Time2a_getHour(self);
    tmTime.tm_mday = CP56Time2a_getDayOfMonth(self);
    tmTime.tm_mon = CP56Time2a_getMonth(self) - 1;
    tmTime.tm_year = CP56Time2a_getYear(self) + 100;

    return tmTime;
}

/** CS104 raw-message callback: prints every sent/received frame as a hex dump prefixed with SEND/RCVD */
static void rawMessageHandler(void *parameter, uint8_t *msg, int msgSize, bool sent) {
    if (sent)
        printf("SEND: ");
    else
        printf("RCVD: ");

    for (int i = 0; i < msgSize; i++) {
        printf("%02x ", msg[i]);
    }

    printf("\n");
}

/** Returns a human-readable quality string for a QualityDescriptor: "GOOD" or a "|"-separated list of active flags (OV, BL, SB, NT, IV) */
static std::string qualityStr(const QualityDescriptor qd) {
    if (qd == IEC60870_QUALITY_GOOD) return "GOOD";
    std::string s;
    if (qd & IEC60870_QUALITY_OVERFLOW) s += "OV|";
    if (qd & IEC60870_QUALITY_BLOCKED) s += "BL|";
    if (qd & IEC60870_QUALITY_SUBSTITUTED) s += "SB|";
    if (qd & IEC60870_QUALITY_NON_TOPICAL) s += "NT|";
    if (qd & IEC60870_QUALITY_INVALID) s += "IV|";
    if (!s.empty()) s.pop_back();
    return s;
}

/** CS104 connection-event callback: logs state transitions and cancels the context on close or failure */
static void connectionHandler(void *parameter, CS104_Connection connection, CS104_ConnectionEvent event) {
    lastEvent = event;

    switch (event) {
        case CS104_CONNECTION_OPENED:
            printf("Connection established\n");
            break;
        case CS104_CONNECTION_CLOSED:
            printf("Connection closed\n");
            context->Cancel();
            reconnecting = false;
            break;
        case CS104_CONNECTION_STARTDT_CON_RECEIVED:
            printf("Received STARTDT_CON\n"); {
                std::unique_lock lock(mtx);
                notified = true;
                cv.notify_one();
            }
            break;
        case CS104_CONNECTION_STOPDT_CON_RECEIVED:
            printf("Received STOPDT_CON\n");
            break;
        case CS104_CONNECTION_FAILED:
            printf("Connection failed\n");
            context->Cancel();
            break;
    }
}

/** Returns true if the IOA filter is disabled (filterIoa == 0) or the object's IOA matches filterIoa */
static bool ioa_matches(InformationObject io) {
    return filterIoa == 0 ||
           InformationObject_getObjectAddress(io) == filterIoa;
}

/** CS101 ASDU-received callback: decodes supported type IDs and prints IOA, quality, value (and timestamp where present) to stdout */
static bool asduReceivedHandler(void *parameter, int address, CS101_ASDU asdu) {
    int i;
    char buffer[50];

    printf("RECVD ASDU type: %s(%i) COT: %s CA: %d elements: %i\n",
           TypeID_toString(CS101_ASDU_getTypeID(asdu)),
           CS101_ASDU_getTypeID(asdu),
           CS101_CauseOfTransmission_toString(CS101_ASDU_getCOT(asdu)),
           CS101_ASDU_getCA(asdu),
           CS101_ASDU_getNumberOfElements(asdu));

    switch (CS101_ASDU_getTypeID(asdu)) {
        case M_SP_NA_1: /* 1 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                SinglePointInformation io =
                        reinterpret_cast<SinglePointInformation>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io)))
                    printf("DI  IOA: %i Q:%02X (%s) value: %i\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           SinglePointInformation_getQuality(io),
                           qualityStr(SinglePointInformation_getQuality(io)).c_str(),
                           SinglePointInformation_getValue(io)
                    );

                SinglePointInformation_destroy(io);
            }
            break;

        case M_DP_NA_1: /* 3 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                DoublePointInformation io =
                        reinterpret_cast<DoublePointInformation>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io)))
                    printf("DI  IOA: %i Q:%02X (%s) value: %i\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           DoublePointInformation_getQuality(io),
                           qualityStr(DoublePointInformation_getQuality(io)).c_str(),
                           DoublePointInformation_getValue(io)
                    );

                DoublePointInformation_destroy(io);
            }
            break;

        case M_ME_NA_1: /* 9 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                MeasuredValueNormalized io =
                        reinterpret_cast<MeasuredValueNormalized>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io)))
                    printf("    IOA: %i Q:%02X (%s) value: %f\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           MeasuredValueNormalized_getQuality(io),
                           qualityStr(MeasuredValueNormalized_getQuality(io)).c_str(),
                           MeasuredValueNormalized_getValue(io)
                    );

                MeasuredValueNormalized_destroy(io);
            }
            break;
        case M_ME_NB_1: /* 11 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                MeasuredValueScaled io =
                        reinterpret_cast<MeasuredValueScaled>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io)))
                    printf("    IOA: %i Q:%02X (%s) value: %d\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           MeasuredValueScaled_getQuality(io),
                           qualityStr(MeasuredValueScaled_getQuality(io)).c_str(),
                           MeasuredValueScaled_getValue(io)
                    );

                MeasuredValueScaled_destroy(io);
            }
            break;

        case M_ME_NC_1: /* 13 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                MeasuredValueShort io =
                        reinterpret_cast<MeasuredValueShort>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io)))
                    printf("    IOA: %i Q:%02X (%s) value: %f\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           MeasuredValueShort_getQuality(io),
                           qualityStr(MeasuredValueShort_getQuality(io)).c_str(),
                           MeasuredValueShort_getValue(io)
                    );

                MeasuredValueShort_destroy(io);
            }
            break;


        case M_ME_ND_1: /* 21 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                MeasuredValueNormalizedWithoutQuality io =
                        reinterpret_cast<MeasuredValueNormalizedWithoutQuality>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io)))
                    printf("    IOA: %i value: %f\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           MeasuredValueNormalizedWithoutQuality_getValue(io)
                    );

                MeasuredValueNormalizedWithoutQuality_destroy(io);
            }
            break;

        case M_SP_TB_1: /* 30 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                SinglePointWithCP56Time2a io =
                        reinterpret_cast<SinglePointWithCP56Time2a>(CS101_ASDU_getElement(asdu, i));

                if (io == nullptr) {
                    printf("DI M_SP_TB_1(30) IOA: %i invalid ASDU\n", i);
                    continue;
                }

                if (ioa_matches(reinterpret_cast<InformationObject>(io))) {
                    int value = SinglePointInformation_getValue(reinterpret_cast<SinglePointInformation>(io));
                    tm tmTime = CP56Time2a_toTm(SinglePointWithCP56Time2a_getTimestamp(io));
                    strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", &tmTime);
                    printf("DI  IOA: %i time: %s.%03d%s%s%s Q:%02X value: %i\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           buffer,
                           CP56Time2a_getMillisecond(SinglePointWithCP56Time2a_getTimestamp(io)),
                           CP56Time2a_isInvalid(SinglePointWithCP56Time2a_getTimestamp(io)) == true ? " !" : " ",
                           CP56Time2a_isSubstituted(SinglePointWithCP56Time2a_getTimestamp(io)) == true ? " S" : " ",
                           CP56Time2a_isSummerTime(SinglePointWithCP56Time2a_getTimestamp(io)) == true ? " +" : " ",
                           SinglePointInformation_getQuality(reinterpret_cast<SinglePointInformation>(io)),
                           value);
                }

                SinglePointWithCP56Time2a_destroy(io);
            }
            break;

        case M_DP_TB_1: /* 31 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                DoublePointWithCP56Time2a io = reinterpret_cast<DoublePointWithCP56Time2a>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io))) {
                    tm tmTime = CP56Time2a_toTm(DoublePointWithCP56Time2a_getTimestamp(io));
                    strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", &tmTime);
                    printf("DI  IOA: %i time: %s.%03d Q:%02X value: %i\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           buffer,
                           CP56Time2a_getMillisecond(DoublePointWithCP56Time2a_getTimestamp(io)),
                           DoublePointInformation_getQuality(reinterpret_cast<DoublePointInformation>(io)),
                           DoublePointInformation_getValue(reinterpret_cast<DoublePointInformation>(io))
                    );
                }

                DoublePointWithCP56Time2a_destroy(io);
            }
            break;

        case M_ME_TD_1: /* 34 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                MeasuredValueNormalizedWithCP56Time2a io = reinterpret_cast<MeasuredValueNormalizedWithCP56Time2a>(CS101_ASDU_getElement(asdu, i));

                if (ioa_matches(reinterpret_cast<InformationObject>(io))) {
                    tm tmTime = CP56Time2a_toTm(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(io));
                    strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", &tmTime);
                    printf("DI  IOA: %i time: %s.%03d Q:%02X value: %f\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           buffer,
                           CP56Time2a_getMillisecond(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(io)),
                           MeasuredValueNormalized_getQuality(reinterpret_cast<MeasuredValueNormalized>(io)),
                           MeasuredValueNormalized_getValue(reinterpret_cast<MeasuredValueNormalized>(io))
                    );
                }

                MeasuredValueNormalizedWithCP56Time2a_destroy(io);
            }
            break;
        case M_ME_TF_1: /* 36 */
            for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
                MeasuredValueShortWithCP56Time2a io = reinterpret_cast<MeasuredValueShortWithCP56Time2a>(CS101_ASDU_getElement(asdu, i));

                if (io == nullptr) {
                    printf("   M_ME_TF_1(36) IOA: %i invalid ASDU\n", i);
                    continue;
                }

                if (ioa_matches(reinterpret_cast<InformationObject>(io))) {
                    const float value = MeasuredValueShort_getValue(reinterpret_cast<MeasuredValueShort>(io));
                    tm tmTime = CP56Time2a_toTm(MeasuredValueShortWithCP56Time2a_getTimestamp(io));
                    strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", &tmTime);
                    printf("    IOA: %i time: %s.%03d%s%s%s Q:%02X value: %f\n",
                           InformationObject_getObjectAddress(reinterpret_cast<InformationObject>(io)),
                           buffer,
                           CP56Time2a_getMillisecond(MeasuredValueShortWithCP56Time2a_getTimestamp(io)),
                           CP56Time2a_isInvalid(MeasuredValueShortWithCP56Time2a_getTimestamp(io)) == true ? " !" : " ",
                           CP56Time2a_isSubstituted(MeasuredValueShortWithCP56Time2a_getTimestamp(io)) == true ? " S" : " ",
                           CP56Time2a_isSummerTime(MeasuredValueShortWithCP56Time2a_getTimestamp(io)) == true ? " +" : " ",
                           MeasuredValueShort_getQuality(reinterpret_cast<MeasuredValueShort>(io)),
                           value);
                }

                MeasuredValueShortWithCP56Time2a_destroy(io);
            }
            break;

        default:
            break;
    }

    return true;
}

/** Main connection loop: connects to the server, starts data transfer, sends a General Interrogation, and reconnects on failure until running is set to false */
void working_cycle(const char *hostname, int tcpPort, int ca) {
    CS104_Connection con = nullptr;

    while (running) {
        printf("Trying to connect to: %s:%i:%d\n", hostname, tcpPort, ca);
        con = CS104_Connection_create(hostname, tcpPort);

        CS104_Connection_setAPCIParameters(con, &currentAPCIParameters);

        CS104_Connection_setConnectionHandler(con, connectionHandler, nullptr);
        CS104_Connection_setASDUReceivedHandler(con, asduReceivedHandler, nullptr);
        CS104_Connection_setRawMessageHandler(con, rawMessageHandler, nullptr);

        if (CS104_Connection_connect(con)) {
            printf("Connected!\n");
            reconnecting = true;

            CS104_Connection_sendStartDT(con);

            Thread_sleep(50);

            printf("Sending general interrogation with the common address: %d\n", ca);
            CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, ca, IEC60870_QOI_STATION);

            while (context->context) {
                Thread_sleep(1000);
            }
            reconnecting = false;
            CS104_Connection_destroy(con);
        } else
            printf("Failed to connect!\n");

        Thread_sleep(5000);
    }

    if (con != nullptr)
        CS104_Connection_destroy(con);
}


/** Entry point: parses command-line flags, applies APCI parameters, installs signal handlers, and runs the connection loop */
int main(const int argc, char **argv) {
    std::string ip;
    int ca = 1;
    int port = IEC_60870_5_104_DEFAULT_PORT;
    int apci_k = currentAPCIParameters.k;
    int apci_w = currentAPCIParameters.w;
    int apci_t0 = currentAPCIParameters.t0;
    int apci_t1 = currentAPCIParameters.t1;
    int apci_t2 = currentAPCIParameters.t2;
    int apci_t3 = currentAPCIParameters.t3;

    bool showVersion = false;

    cppflags::FlagSet flags;
    flags.SetPreamble(std::string("iec104_test_client version ") + VERSION);
    flags.Bool(  "version", &showVersion,                     "Print version and exit");
    flags.String("host",    &ip,   "",                        "Server IP address");
    flags.Int(   "port",    &port, IEC_60870_5_104_DEFAULT_PORT, "TCP port to connect to");
    flags.Int("ca", &ca, 1, "Common address (station)");
    flags.Int("ioa", &filterIoa, 0, "Show only this IOA (0 = all)");
    flags.Int("k", &apci_k, currentAPCIParameters.k, "APCI: max unacknowledged I-frames (k)");
    flags.Int("w", &apci_w, currentAPCIParameters.w, "APCI: ACK after w received I-frames (w)");
    flags.Int("t0", &apci_t0, currentAPCIParameters.t0, "APCI: connection timeout in seconds (t0)");
    flags.Int("t1", &apci_t1, currentAPCIParameters.t1, "APCI: send/test APDU timeout in seconds (t1)");
    flags.Int("t2", &apci_t2, currentAPCIParameters.t2, "APCI: ACK timeout in seconds (t2)");
    flags.Int("t3", &apci_t3, currentAPCIParameters.t3, "APCI: test frame timeout in seconds (t3)");

    try {
        flags.Parse(argc, argv);
    } catch (const cppflags::ParseError &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        flags.printUsage(argv[0]);
        return 1;
    }

    if (showVersion) {
        printf("iec104_test_client version %s\n", VERSION);
        return 0;
    }

    if (ip.empty()) {
        fprintf(stderr, "Error: --host is required\n");
        flags.printUsage(argv[0]);
        return 1;
    }

    currentAPCIParameters.k = apci_k;
    currentAPCIParameters.w = apci_w;
    currentAPCIParameters.t0 = apci_t0;
    currentAPCIParameters.t1 = apci_t1;
    currentAPCIParameters.t2 = apci_t2;
    currentAPCIParameters.t3 = apci_t3;

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    signal(SIGINT, sigint_handler);

    working_cycle(ip.c_str(), static_cast<uint16_t>(port), ca);

    printf("exit\n");
}
