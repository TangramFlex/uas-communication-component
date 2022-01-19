#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <cstdint>
#include <string>
#include <vector>

// These are the header files for the messages needed from the LMCP generic CSI
#include "AirVehicleState.hpp"
#include "AirVehicleConfiguration.hpp"

// These headers are for the serializer to use (LMCP format) and the factory
// object that provides new objects based on type
#include "LMCPSerializer.h"
#include "DerivedEntityFactory.hpp"

#define HEADER_SIZE 8
#define CHECKSUM_SIZE 4

/* Sends bytes contained in the buffer provided. Does not return until all bytes
 * are sent or an error occurs.
 * \param data The buffer of bytes to be sent.
 * \param numBytes The number of bytes to be sent.
 * \param fd The file descriptor to use for the send.
 * \returns The number of bytes sent on success or -1 on error.
 */
static int sendBytes(const unsigned char *data, uint32_t numBytes, int fd) {

    uint32_t bytesSent = 0;
    while(bytesSent != numBytes) {
        int status = write(fd, &data[bytesSent], numBytes - bytesSent);
        if (status <= 0) {
            return -1;
        } else {
            bytesSent += status;
        }
    }

    return numBytes;
}

/* Sends a serialized LMCP message wrapped to be compatible with the needs of
 * the OpenAMASE simulator.
 * \param data The serialized data to be sent.
 * \param messageSize The size of the serialized data in bytes.
 * \param messageName The name of the message (fully qualified) being sent.
 * \param fd The file descriptor to use for the send.
 * \returns The number of bytes sent on success or -1 on error.
 */
static int sendMessage(const unsigned char *data, uint32_t messageSize,
    const char *messageName, int fd) {

    std::string attributes = messageName;
    attributes += "$lmcp|";
    attributes += messageName;
    attributes += "||0|0$";

    char buf[256];
    snprintf(buf, 256, "%lu", attributes.size() + messageSize + HEADER_SIZE +
        CHECKSUM_SIZE);
    std::string sentinel = "+=+=+=+=";
    sentinel += buf;
    sentinel += "#@#@#@#@";

    std::vector<uint8_t> addressedPayload;
    std::copy(attributes.begin(), attributes.end(),
        std::back_inserter(addressedPayload));
    std::copy(data, data + messageSize, std::back_inserter(addressedPayload));

    uint32_t val = 0;
    for (uint32_t i = 0; i < addressedPayload.size(); i++) {
        val += (addressedPayload[i] & 0xFF);
    }

    std::vector<uint8_t> frontBytes;
    std::copy(sentinel.begin(), sentinel.end(), std::back_inserter(frontBytes));
    std::copy(addressedPayload.begin(), addressedPayload.end(),
        std::back_inserter(frontBytes));

    std::string footer = "!%!%!%!%";
    snprintf(buf, 256, "%u", val);
    footer += buf;
    footer += "?^?^?^?^";

    std::vector<uint8_t> completeMessage;
    std::copy(frontBytes.begin(), frontBytes.end(),
        std::back_inserter(completeMessage));
    std::copy(footer.begin(), footer.end(), std::back_inserter(completeMessage));

    fprintf(stderr, "Sending message of %lu byets.\n", completeMessage.size());
    return sendBytes(&completeMessage[0], completeMessage.size(), fd);
}

/* This is the entry point for a program that creates and sends an LMCP
 * AirConfigurationMessage followed by a continous stream of LMCP
 * AirVehicleState messages. The result should be that a "Tangram UAV"
 * UAS is shown on the OpenAMASE display and its position updated at a 1Hz
 * rate.
 */
int main(int argc, char *argv[]) {

    // Talking to OpenAMASE is done using a regular old TCP stream socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf(stderr, "Failed to create socket (%d) (%s).\n", fd,
            strerror(errno));
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    // OpenAMASE sets up a server socket on 0.0.0.0:5555
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5555);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);
    if (connect(fd, (const struct sockaddr *) &addr, sizeof(addr)) != 0) {
        fprintf(stderr, "Failed to connect socket (%s).\n", strerror(errno));
        close(fd);
        return 1;
    }
    fprintf(stderr, "Connected to server.\n");

    // To serialize messages, we need an LMCP CSI entity factory and serializer
    tangram::genericapi::DerivedEntityFactory entityFactory;
    tangram::serializers::LMCPSerializer lmcpSerializer(&entityFactory);

    // Create and populate an AirVehicleConfiguration message, which tells
    // OpenAMASE about our platform. If this message is successfully received
    // and processed by OpenAMASE the "Tangram UAV" platform will appear on the
    // right hand side of the screen.
    fprintf(stderr, "Starting AirVehicleConfiguration\n");
    afrl::cmasi::AirVehicleConfiguration avc;
    avc.setID(600, false);
    avc.setAffiliation("Tangram Flex", false);
    avc.setLabel("Tangram UAV", false);
    avc.setNominalSpeed(25.0, false);
    avc.setNominalAltitude(800.0f, false);
    avc.setNominalAltitudeType("MSL");
    avc.setMinimumSpeed(18.044F, false);
    avc.setMaximumSpeed(33.223f, false);
    avc.setMinimumAltitude(0.0f, false);
    avc.setMinAltitudeType("AGL", false);
    avc.setMaximumAltitude(100000.0f, false);
    avc.setMaxAltitudeType("MSL", false);
    avc.addToAvailableLoiterTypes(afrl::cmasi::LoiterType::EnumItem::Circular,
        false);
    avc.addToAvailableLoiterTypes(
        afrl::cmasi::LoiterType::EnumItem::FigureEight, false);
    avc.addToAvailableTurnTypes(afrl::cmasi::TurnType::EnumItem::TurnShort,
        false);
    avc.addToAvailableTurnTypes(afrl::cmasi::TurnType::EnumItem::FlyOver,
        false);

    // Serialize and send the AirVehicleConfiguration message
    fprintf(stderr, "Serializing AirVehicleConfiguration\n");
    std::vector<uint8_t> avcBytes;
    if (!lmcpSerializer.serialize(avc, avcBytes)) {
        fprintf(stderr, "Failed to serialize message.\n");
        return 1;
    }
    fprintf(stderr, "Sending AirVehicleConfiguration (%lu bytes)\n",
        avcBytes.size());
    if (sendMessage(&avcBytes[0], avcBytes.size(),
        "afrl.cmasi.AirVehicleConfiguration", fd) == -1) {
        fprintf(stderr, "ERROR SENDING AirVehicleConfiguration (%s)\n",
            strerror(errno));
    }
    printf("Sent AirVehicleConfiguration message.\n");

    // Wait 3 seconds
    sleep(3);

    // Setup some starter values for the AirVehicleState messages that will be
    // streamed to OpenAMASE
    double longitude = -121.00538540744407;
    double latitude = 45.30724068719066;
    double altitude = 755.0;
    double heading = 90.0;
    double energy = 99.0;

    // Start setting up the AirVehicleState message
    afrl::cmasi::AirVehicleState avs;
    avs.setID(600, false);
    avs.getLocation()->setLatitude(latitude, false);
    avs.getLocation()->setLongitude(longitude, false);
    avs.getLocation()->setAltitude(altitude, false);
    avs.getLocation()->setAltitudeType(afrl::cmasi::AltitudeType::EnumItem::MSL,
        false);
    avs.setAirspeed(19.4423, false);
    //avs.setVerticalSpeed(0.0f, false);
    //avs.setWindSpeed(19.3f, false);
    //avs.setWindDirection(29.332f, false);
    avs.setPitch(-.5, false);
    avs.setRoll(2.442, false);
    avs.setMode("FlightDirector");
    avs.setEnergyAvailable(energy, false);

    // Infinite loop to send AirVehicleState messages until we are forcibly
    // stopped
    int count = 0;
    double multiplier = 1.0;
    while (1) {

        fprintf(stderr, "Starting AirVehicleState\n");
        avs.getLocation()->setLatitude(latitude, false);
        avs.getLocation()->setLongitude(longitude, false);
        avs.getLocation()->setAltitude(altitude, false);
        avs.getLocation()->setAltitudeType(
            afrl::cmasi::AltitudeType::EnumItem::MSL, false);
        avs.setEnergyAvailable(energy, false);
        avs.setHeading(heading, false);

        fprintf(stderr, "Serializing AirVehicleState\n");
        std::vector<uint8_t> avsBytes;
        if (!lmcpSerializer.serialize(avs, avsBytes)) {
            fprintf(stderr, "Failed to serialize message.\n");
            return 1;
        }
        fprintf(stderr, "Sending AirVehicleState (%lu bytes)\n",
            avsBytes.size());
        if (sendMessage(&avsBytes[0], avsBytes.size(),
            "afrl.cmasi.AirVehicleState", fd) == -1) {
            fprintf(stderr, "ERROR SENDING AirVehicleState (%s)\n",
                strerror(errno));
        }
        fprintf(stderr, "Sent AirVehicleState message.\n");

        sleep(1);

        // bump the latitude, altitude and energy; latitude depends on where
        // we are, it may go up or down
        longitude += 0.0021223 * multiplier;
        altitude += 1.2;
        energy -= 0.15;

        // every 30 updates, swap the direction of travel
        count++;
        if (count == 30) {
            count = 0;
            if (multiplier < 0.0) {
                heading = 90.0;
            } else {
                heading = 270.0;
            }
            multiplier *= -1.0;
        }
    }

    close(fd);

    return 0;
}
