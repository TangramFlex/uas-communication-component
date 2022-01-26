// This file is for demonstrating a Tangram Pro generated LMCP component software 
// interface communciating with OpenAMASE. OpenAMASE will display the status
// of a simulated UAV (ID=600) in real-time.
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

// These are the header files for the messages needed from the LMCP generic CSI
#include "AirVehicleState.hpp"
#include "AirVehicleConfiguration.hpp"

// These headers are for the LMCP serializer and the factory object that
// provides new objects based on type
#include "LMCPSerializer.h"
#include "afrl_cmasi_DerivedEntityFactory.hpp"
afrl::cmasi::DerivedEntityFactory *lmcpEntityFactory;
tangram::serializers::LMCPSerializer *lmcpSerializer;

// Some constants for the LMCP TCP wrapper. To send LMCP to OpenAMASE via TCP
// stream socket, there's an additional wrapper required around the serialized
// LMCP message.
#define HEADER_SIZE 8
#define CHECKSUM_SIZE 4

// The file descriptor for the TCP socket to OpenAMASE
static int openAMASESocket = -1;

// Performs an on-demand connect to OpenAMASE via TCP stream socket
static int connectToOpenAMASE() {
    
    // Talking to OpenAMASE is done using a regular old TCP stream socket
    openAMASESocket = socket(AF_INET, SOCK_STREAM, 0);
    if (openAMASESocket == -1) {
        fprintf(stderr, "Failed to create socket (%d) (%s).\n", openAMASESocket,
            strerror(errno));
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);

    // OpenAMASE sets up a server socket on 0.0.0.0:5555
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5555);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);
    if (connect(openAMASESocket, (const struct sockaddr *) &addr, sizeof(addr))
        != 0) {
        fprintf(stderr, "Failed to connect socket (%s).\n", strerror(errno));
        close(openAMASESocket);
        openAMASESocket = -1;
        return -1;
    }
    fprintf(stderr, "Connected to OpenAMASE.\n");

    return 0;
}

// Performs a blocking write of the given data buffer to the OpenAMASE TCP
// stream socket.
static int sendBytesToOpenAMASE(const unsigned char *data, uint32_t numBytes) {

    // This is a connect-on-demand setup so if the connection to OpenAMASE
    // hasn't already been made, do that now
    if (openAMASESocket == -1) {
        if (connectToOpenAMASE() != 0) {
            return -1;
        }
    }

    // Socket send loop, doesn't exit until all bytes given are sent or an
    // error is detected
    uint32_t bytesSent = 0;
    while(bytesSent != numBytes) {
        int status = write(openAMASESocket, &data[bytesSent], numBytes -
            bytesSent);
        if (status <= 0) {
            close(openAMASESocket);
            openAMASESocket = -1;
            return -1;
        } else {
            bytesSent += status;
        }
    }

    return numBytes;
}

// Sends a complete LMCP message to OpenAMASE property wrapping it to be
// compatible with the TCP server setup of OpenAMASE. It will perform the
// initial connection if needed and send the initial AirVehicleConfiguration
// message as well.
static int sendMessageToOpenAMASE(const unsigned char *data, uint32_t messageSize,
    const char *messageName) {
        
    // this code is a fairly direct port of Java code found in the OpenUxAS
    // LMCP Java library
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

    int status = sendBytesToOpenAMASE(&completeMessage[0], completeMessage.size());
    if (status == 0) {
        fprintf(stderr, "Failed to send message %s to OpenAMASE!\n", messageName); 
    }
    return status;
}

// Construct and build and AirVehicleConfiguration message suitable for our
// scenario, then send it to OpenAMASE.

static int sendAirVehicleConfiguration() {

    // Note that the AirVehicleConfiguration ID is set to 600. This is what 
    // the new UAV will be seen as in the OpenAMASE application. Also note 
    // that the AirVehicleState messages to follow must have that same ID 
    // value set so that they can be matched up to the configuration message 
    // for that vehicle.
    afrl::cmasi::AirVehicleConfiguration avc;
    avc.setID(600, false);
    avc.setAffiliation("Tangram Flex", false);
    avc.setLabel("Tangram UAV", false);
    avc.setNominalSpeed(25.0, false);
    avc.setNominalAltitude(800.0f, false);
    avc.setNominalAltitudeType("MSL");
    avc.setMinimumSpeed(18.044F, false);
    avc.setMaximumSpeed(33.223f, false);
    avc.setMinimumAltitude(0.0f);
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

    printf("Serializing AirVehicleConfiguration\n");
    std::vector<uint8_t> avcBytes;
    if (!lmcpSerializer->serialize(avc, avcBytes)) {
        fprintf(stderr, "Failed to serialize LMCP message.\n");
        return 1;
    }
    printf("Sending AirVehicleConfiguration (%lu bytes)\n",
        avcBytes.size());
    if (sendMessageToOpenAMASE(&avcBytes[0], avcBytes.size(),
        "afrl.cmasi.AirVehicleConfiguration") == -1) {
        fprintf(stderr, "ERROR SENDING AirVehicleConfiguration (%s)\n",
            strerror(errno));
    }
    printf("Sent AirVehicleConfiguration message.\n");

    return 0;
}

// Initializes the needed serializers. We have to use an LMCP serializer to send
// messages to OpenAMASE
static int initializeSerializer() {
    lmcpEntityFactory = new afrl::cmasi::DerivedEntityFactory();
    lmcpSerializer = new tangram::serializers::LMCPSerializer(lmcpEntityFactory);

    return 0;
}

// Cleans up previously created serializers
static void destroySerializer() {
    delete lmcpSerializer;
    delete lmcpEntityFactory;
}

int main(int argc, char *argv[]) {

    // Setup the serializers needed
    initializeSerializer();
    printf("Initialized LMCP serializer.\n");

    // Send AirVehicleConfiguration to OpenAMASE
    sendAirVehicleConfiguration();

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
        if (!lmcpSerializer->serialize(avs, avsBytes)) {
            fprintf(stderr, "Failed to serialize message.\n");
            return 1;
        }
        fprintf(stderr, "Sending AirVehicleState (%lu bytes)\n",
            avsBytes.size());
        if (sendMessageToOpenAMASE(&avsBytes[0], avsBytes.size(),
            "afrl.cmasi.AirVehicleState") == -1) {
            fprintf(stderr, "ERROR SENDING AirVehicleState (%s)\n",
                strerror(errno));
        }
        fprintf(stderr, "Sent AirVehicleState message.\n");

        sleep(1);

        // Bump the latitude, altitude and energy; latitude depends on where
        // we are, it may go up or down
        longitude += 0.0021223 * multiplier;
        altitude += 1.2;
        energy -= 0.15;

        // Every 30 updates, swap the direction of travel
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

    // Cleanup before leaving
    if (openAMASESocket != -1) {
        close(openAMASESocket);
    }
    destroySerializer();

    return 0;
}
