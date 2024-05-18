#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <vector>
#include <algorithm> 
#include <filesystem>
#include <cstdint>
#include <map>
#include <bitset> 
#include <string>
#include <queue>
#include <variant> 
#include <numeric> 
#include <utility> 
#include <array> 
#include <sstream> 
#include <string>
#include "TextConverter.hpp"
#include "CSVConverter.hpp"
#include "TemplateLibrary/ByteBuffer.hpp"
#include "Implementation/FaPacketProtocol.hpp"
#include "Implementation/BinaryHeader.hpp"
#include "Implementation/ASCIIPacketProtocol.hpp"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

class Parser {
private: 
    struct SensorData {
        std::vector<std::string> ASCIIMeasurements;
        std::vector<std::string> ASCIIPrinterFriendly;
        std::vector<std::string> binaryMeasurements;
        
        SensorData() = default; // initialize empty vectors!
    };

    std::ifstream reader;
    std::string type;
    std::filesystem::path filePath; 
    SensorData parsedSensorData; 
    std::vector<std::pair<std::string, std::string>> binmeasurementsFound;
    std::vector<std::pair<std::string, std::string>> asciimeasurementsFound;

    std::string removeNonPrintableCharacters(const std::string& input) {
        std::string result;
        for (char c : input) {
            if (isprint(static_cast<unsigned char>(c)) || c == '\r' || c == '\n') {
                result += c;
            }
        }

        if (result.find("$VFJA") != std::string::npos) {
            return result.substr(4);
        }
        return result;
    }
    void readFileContents() {
        // Read and digest binary content
        char byte;
        std::stringstream textStream;

        std::stringstream asciibinstorage; // Keeps the binary bits for instantiating to the ByteHeader --> VectorNav's proprietary function

        while (reader.read(&byte, sizeof(byte))) {
            // Two-Digit Hexadecimal
            std::stringstream hexStream;
            hexStream << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(byte);
            
            if (hexStream.str() == "fffffffa") {
                std::string temp = asciibinstorage.str();
                std::string substrascii = "24 " + temp;
                std::transform(substrascii.begin(), substrascii.end(), substrascii.begin(), ::toupper);
                if (substrascii != "24 ") {
                    parsedSensorData.ASCIIMeasurements.push_back(substrascii);
                }

                asciibinstorage.str("");

                textStream.str("");
                textStream << hexStream.str().substr(6, 2); // Grabs the first two characters

                while (reader.read(&byte, sizeof(byte)) && byte != 0x24) {
                    hexStream.str("");
                    hexStream << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(static_cast<unsigned char>(byte));

                    if (hexStream.str() == "24") {
                        break; // Exit the loop if the hexadecimal value is "24"
                    }

                    if (textStream.str() == "fffffffa") {
                        std::string substring = textStream.str();
                        std::transform(substring.begin(), substring.end(), substring.begin(), ::toupper);

                        parsedSensorData.binaryMeasurements.push_back(substring);
                        textStream.str("");
                        textStream << hexStream.str().substr(6, 2);
                    }
                    else {
                        textStream << " " << hexStream.str();
                    }
                }

                std::string substring = textStream.str();
                std::transform(substring.begin(), substring.end(), substring.begin(), ::toupper);

                parsedSensorData.binaryMeasurements.push_back(substring);
                textStream.str("");
                textStream << "$";
                continue;
            }
            else {
                asciibinstorage << hexStream.str() << " ";
            }


            // Hexadecimal --> Integer
            int hexValue;
            hexStream >> hexValue;

            // Integer --> Char
            char character = static_cast<char>(hexValue);

            // Default Separator (if it finds an ASCII '\n')
            if (character == '\r') {
                parsedSensorData.ASCIIPrinterFriendly.push_back(removeNonPrintableCharacters(textStream.str()));

                // Clear the stringstream for the next line
                textStream.str(""); 
            }
            else {
                // Append the character to textStream
                textStream << character;
            }
        }
    }

    std::vector<uint8_t> rawbytesinsertor(const std::string& hexString) {
        std::vector<uint8_t> bytes;
        std::istringstream hexStream(hexString);

        std::transform(std::istream_iterator<std::string>(hexStream),
            std::istream_iterator<std::string>(),
            std::back_inserter(bytes),
            [](const std::string& hexPair) {
                try {
                    return static_cast<uint8_t>(std::stoi(hexPair, nullptr, 16));
                }
                catch (const std::exception& e) {
                    std::cerr << "Error converting hex string to bytes: " << e.what() << std::endl;
                    return static_cast<uint8_t>(0); // Default value in case of error
                }
            });

        return bytes;
    }

    template <typename T>
    std::string to_string(const T& value) {
        std::ostringstream ss;
        ss << value;
        return ss.str();
    }

    void asciiParser(std::vector<std::string> asciiData) {
        for (const auto& line : asciiData) {
            VN::ByteBuffer currentByteStream(line.size());
            std::vector<uint8_t> dataBytes = rawbytesinsertor(line);
            currentByteStream.put(dataBytes.data(), dataBytes.size());
            auto findPacketResult = VN::AsciiPacketProtocol::findPacket(currentByteStream, 0);

            if (findPacketResult.validity == VN::AsciiPacketProtocol::Validity::Valid) {
                auto compositeData = VN::AsciiPacketProtocol::parsePacket(currentByteStream, 0, findPacketResult.metadata);
                auto enabledMeasures = VN::AsciiPacketProtocol::asciiHeaderToMeasHeader(findPacketResult.metadata.header);

                if (compositeData->time.TimeStartup.has_value()) { asciimeasurementsFound.push_back(std::make_pair("TimeStartup", to_string(compositeData->time.TimeStartup.value()))); }
                if (compositeData->time.TimeGps.has_value()) { asciimeasurementsFound.push_back(std::make_pair("TimeGPS", to_string(compositeData->time.TimeGps.value()))); }
                if (compositeData->time.GpsTow.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GpsTow", to_string(compositeData->time.GpsTow.value()))); }
                if (compositeData->time.GpsWeek.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GpsWeek", to_string(compositeData->time.GpsWeek.value()))); }
                if (compositeData->time.TimeGpsPps.has_value()) { asciimeasurementsFound.push_back(std::make_pair("TimeGpsPps", to_string(compositeData->time.TimeGpsPps.value()))); }
                if (compositeData->time.TimeUtc.has_value()) { asciimeasurementsFound.push_back(std::make_pair("TimeUtc", to_string(2000 + compositeData->time.TimeUtc.value().Year) + "-" + to_string(compositeData->time.TimeUtc.value().Month) + "-" + to_string(compositeData->time.TimeUtc.value().Day) + "-" + to_string(compositeData->time.TimeUtc.value().Hour) + "-" + to_string(compositeData->time.TimeUtc.value().Minute) + "-" + to_string(compositeData->time.TimeUtc.value().Second) + "-" + to_string(compositeData->time.TimeUtc.value().FracSec))); }
                if (compositeData->time.SyncInCnt.has_value()) { asciimeasurementsFound.push_back(std::make_pair("SyncInCnt", to_string(compositeData->time.SyncInCnt.value()))); }
                if (compositeData->time.SyncOutCnt.has_value()) { asciimeasurementsFound.push_back(std::make_pair("SyncOutCnt", to_string(compositeData->time.SyncOutCnt.value()))); }
                if (compositeData->time.TimeStatus.has_value()) { asciimeasurementsFound.push_back(std::make_pair("TimeStatus", to_string(compositeData->time.TimeStatus.value()))); }

                if (compositeData->imu.ImuStatus.has_value()) { asciimeasurementsFound.push_back(std::make_pair("ImuStatus", to_string(compositeData->imu.ImuStatus.value()))); }
                if (compositeData->imu.UncompMag.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompMag X-axis", to_string(compositeData->imu.UncompMag.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompMag Y-axis", to_string(compositeData->imu.UncompMag.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompMag Z-axis", to_string(compositeData->imu.UncompMag.value()[2])));
                }
                if (compositeData->imu.UncompAccel.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompAccel X-axis", to_string(compositeData->imu.UncompAccel.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompAccel Y-axis", to_string(compositeData->imu.UncompAccel.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompAccel Z-axis", to_string(compositeData->imu.UncompAccel.value()[2])));
                }
                if (compositeData->imu.UncompGyro.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompGyro X-axis", to_string(compositeData->imu.UncompGyro.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompGyro Y-axis", to_string(compositeData->imu.UncompGyro.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuUncompGyro Z-axis", to_string(compositeData->imu.UncompGyro.value()[2])));
                }
                if (compositeData->imu.Temperature.has_value()) { asciimeasurementsFound.push_back(std::make_pair("ImuTemperature", to_string(compositeData->imu.Temperature.value()))); }
                if (compositeData->imu.Pressure.has_value()) { asciimeasurementsFound.push_back(std::make_pair("ImuPressure", to_string(compositeData->imu.Pressure.value()))); }
                if (compositeData->imu.DeltaTheta.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuDeltaTheta dtime", to_string(compositeData->imu.DeltaTheta.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuDeltaTheta X-axis", to_string(compositeData->imu.DeltaTheta.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuDeltaTheta Y-axis", to_string(compositeData->imu.DeltaTheta.value()[2])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuDeltaTheta Z-axis", to_string(compositeData->imu.DeltaTheta.value()[3])));
                }
                if (compositeData->imu.DeltaVel.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuDeltaVel X-axis", to_string(compositeData->imu.DeltaVel.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuDeltaVel Y-axis", to_string(compositeData->imu.DeltaVel.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuDeltaVel Z-axis", to_string(compositeData->imu.DeltaVel.value()[2])));
                }
                if (compositeData->imu.Mag.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuMag X-axis", to_string(compositeData->imu.Mag.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuMag Y-axis", to_string(compositeData->imu.Mag.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuMag Z-axis", to_string(compositeData->imu.Mag.value()[2])));
                }
                if (compositeData->imu.Accel.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuAccel X-axis", to_string(compositeData->imu.Accel.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuAccel Y-axis", to_string(compositeData->imu.Accel.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuAccel Z-axis", to_string(compositeData->imu.Accel.value()[2])));
                }
                if (compositeData->imu.AngularRate.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("ImuAngularRate X-axis", to_string(compositeData->imu.AngularRate.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuAngularRate Y-axis", to_string(compositeData->imu.AngularRate.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("ImuAngularRate Z-axis", to_string(compositeData->imu.AngularRate.value()[2])));
                }
                if (compositeData->imu.SensSat.has_value())  { asciimeasurementsFound.push_back(std::make_pair("SensSat", to_string(compositeData->imu.SensSat.value()))); }

                if (compositeData->gnss.TimeUtc.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssTimeUtc (Year-Month-Day-Hour-Min-Sec-Ms)", to_string(2000 + compositeData->gnss.TimeUtc.value().Year) + "-" + to_string(compositeData->gnss.TimeUtc.value().Month) + "-" + to_string(compositeData->gnss.TimeUtc.value().Day) + "-" + to_string(compositeData->gnss.TimeUtc.value().Hour) + "-" + to_string(compositeData->gnss.TimeUtc.value().Minute) + "-" + to_string(compositeData->gnss.TimeUtc.value().Second) + "-" + to_string(compositeData->gnss.TimeUtc.value().FracSec))); }
                if (compositeData->gnss.GpsTow.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssGpsTow", to_string(compositeData->gnss.GpsTow.value()))); }
                if (compositeData->gnss.GpsWeek.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssGpsWeek", to_string(compositeData->gnss.GpsWeek.value()))); }
                if (compositeData->gnss.NumSats.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssNumSats", to_string(compositeData->gnss.NumSats.value()))); }
                if (compositeData->gnss.GnssFix.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssFix", to_string(compositeData->gnss.GnssFix.value()))); }
                if (compositeData->gnss.GnssPosLla.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosLla Latitude", to_string(compositeData->gnss.GnssPosLla.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosLla Longitude", to_string(compositeData->gnss.GnssPosLla.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosLla Altitude", to_string(compositeData->gnss.GnssPosLla.value()[2])));
                }
                if (compositeData->gnss.GnssPosEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosEcef pos[0]", to_string(compositeData->gnss.GnssPosEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosEcef pos[1]", to_string(compositeData->gnss.GnssPosEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosEcef pos[2]", to_string(compositeData->gnss.GnssPosEcef.value()[2])));
                }
                if (compositeData->gnss.GnssVelNed.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssVelNed North Axis", to_string(compositeData->gnss.GnssVelNed.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssVelNed East Axis", to_string(compositeData->gnss.GnssVelNed.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssVelNed Down Axis", to_string(compositeData->gnss.GnssVelNed.value()[2])));
                }
                if (compositeData->gnss.GnssVelEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssVelEcef X-Axis", to_string(compositeData->gnss.GnssVelEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssVelEcef Y-Axis", to_string(compositeData->gnss.GnssVelEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssVelEcef Z-Axis", to_string(compositeData->gnss.GnssVelEcef.value()[2])));
                }
                if (compositeData->gnss.GnssPosUncertainty.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosUncertainty North Axis", to_string(compositeData->gnss.GnssPosUncertainty.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosUncertainty East Axis", to_string(compositeData->gnss.GnssPosUncertainty.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("GnssPosUncertainty Down Axis", to_string(compositeData->gnss.GnssPosUncertainty.value()[2])));
                }
                if (compositeData->gnss.GnssVelUncertainty.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssVelUncertainty", to_string(compositeData->gnss.GnssVelUncertainty.value()))); break; }
                if (compositeData->gnss.GnssTimeUncertainty.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssTimeUncertainty", to_string(compositeData->gnss.GnssTimeUncertainty.value()))); break; }
                if (compositeData->gnss.GnssTimeInfo.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssTimeInfo Status", to_string(compositeData->gnss.GnssTimeInfo.value().GnssTimeStatus)));
                    asciimeasurementsFound.push_back(std::make_pair("GnssTimeInfo Leap Seconds", to_string(compositeData->gnss.GnssTimeInfo.value().LeapSeconds)));
                }
                if (compositeData->gnss.GnssDop.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssDop gDOP", to_string(compositeData->gnss.GnssDop.value().Gdop)));
                    asciimeasurementsFound.push_back(std::make_pair("GnssDop pDOP", to_string(compositeData->gnss.GnssDop.value().Pdop)));
                    asciimeasurementsFound.push_back(std::make_pair("GnssDop tDOP", to_string(compositeData->gnss.GnssDop.value().Tdop)));
                    asciimeasurementsFound.push_back(std::make_pair("GnssDop vDOP", to_string(compositeData->gnss.GnssDop.value().Vdop)));
                    asciimeasurementsFound.push_back(std::make_pair("GnssDop hDOP", to_string(compositeData->gnss.GnssDop.value().Hdop)));
                    asciimeasurementsFound.push_back(std::make_pair("GnssDop nDOP", to_string(compositeData->gnss.GnssDop.value().Ndop)));
                    asciimeasurementsFound.push_back(std::make_pair("GnssDop eDOP", to_string(compositeData->gnss.GnssDop.value().Edop)));
                }
                if (compositeData->gnss.GnssSatInfo.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssSatInfo GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Sys.begin(), compositeData->gnss.GnssSatInfo.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssSatInfo Space Vehicle ID", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().SvId.begin(), compositeData->gnss.GnssSatInfo.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssSatInfo Flags", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Flags.begin(), compositeData->gnss.GnssSatInfo.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssSatInfo Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Cno.begin(), compositeData->gnss.GnssSatInfo.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssSatInfo Quality Indicator", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Qi.begin(), compositeData->gnss.GnssSatInfo.value().Qi.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssSatInfo Elevation", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().El.begin(), compositeData->gnss.GnssSatInfo.value().El.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssSatInfo Azimuth", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Az.begin(), compositeData->gnss.GnssSatInfo.value().Az.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                }
                if (compositeData->gnss.GnssRawMeas.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Sys.begin(), compositeData->gnss.GnssRawMeas.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Space Vehicle ID", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().SvId.begin(), compositeData->gnss.GnssRawMeas.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Frequency", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Freq.begin(), compositeData->gnss.GnssRawMeas.value().Freq.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Channel", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Chan.begin(), compositeData->gnss.GnssRawMeas.value().Chan.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Slot ID", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Slot.begin(), compositeData->gnss.GnssRawMeas.value().Slot.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Cno.begin(), compositeData->gnss.GnssRawMeas.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Flags", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Flags.begin(), compositeData->gnss.GnssRawMeas.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Pseudorange", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Pr.begin(), compositeData->gnss.GnssRawMeas.value().Pr.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Carrier Phase", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Cp.begin(), compositeData->gnss.GnssRawMeas.value().Cp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("GnssRawMeas Doppler Measurement", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Dp.begin(), compositeData->gnss.GnssRawMeas.value().Dp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                }
                if (compositeData->gnss.GnssStatus.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssStatus", to_string(compositeData->gnss.GnssStatus.value()))); break; }
                if (compositeData->gnss.GnssAltMSL.has_value()) { asciimeasurementsFound.push_back(std::make_pair("GnssAltMSL", to_string(compositeData->gnss.GnssAltMSL.value()))); break; }


                if (compositeData->attitude.Ypr.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Ypr Yaw", to_string(compositeData->attitude.Ypr.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Ypr Pitch", to_string(compositeData->attitude.Ypr.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Ypr Roll", to_string(compositeData->attitude.Ypr.value()[2])));
                }
                if (compositeData->attitude.Quaternion.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Quaternion qtn[0]", to_string(compositeData->attitude.Quaternion.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Quaternion qtn[1]", to_string(compositeData->attitude.Quaternion.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Quaternion qtn[2]", to_string(compositeData->attitude.Quaternion.value()[2])));
                    asciimeasurementsFound.push_back(std::make_pair("Quaternion qtn[3]", to_string(compositeData->attitude.Quaternion.value()[2])));
                }
                if (compositeData->attitude.Dcm.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[0]", to_string(compositeData->attitude.Quaternion.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[1]", to_string(compositeData->attitude.Quaternion.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[2]", to_string(compositeData->attitude.Quaternion.value()[2])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[3]", to_string(compositeData->attitude.Quaternion.value()[3])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[4]", to_string(compositeData->attitude.Quaternion.value()[4])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[5]", to_string(compositeData->attitude.Quaternion.value()[5])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[6]", to_string(compositeData->attitude.Quaternion.value()[6])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[7]", to_string(compositeData->attitude.Quaternion.value()[7])));
                    asciimeasurementsFound.push_back(std::make_pair("Dcm dcm[8]", to_string(compositeData->attitude.Quaternion.value()[8])));
                }
                if (compositeData->attitude.MagNed.has_value()) {

                    asciimeasurementsFound.push_back(std::make_pair("MagNed North Axis", to_string(compositeData->attitude.MagNed.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("MagNed East Axis", to_string(compositeData->attitude.MagNed.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("MagNed Down Axis", to_string(compositeData->attitude.MagNed.value()[2])));
                }
                if (compositeData->attitude.AccelNed.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("AccelNed North Axis", to_string(compositeData->attitude.AccelNed.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("AccelNed East Axis", to_string(compositeData->attitude.AccelNed.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("AccelNed Down Axis", to_string(compositeData->attitude.AccelNed.value()[2])));
                }
                if (compositeData->attitude.LinBodyAcc.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("LinBodyAcc X-Axis", to_string(compositeData->attitude.LinBodyAcc.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("LinBodyAcc Y-Axis", to_string(compositeData->attitude.LinBodyAcc.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("LinBodyAcc Z-Axis", to_string(compositeData->attitude.LinBodyAcc.value()[2])));
                }
                if (compositeData->attitude.LinAccelNed.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("LinAccelNed North Axis", to_string(compositeData->attitude.LinAccelNed.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("LinAccelNed East Axis", to_string(compositeData->attitude.LinAccelNed.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("LinAccelNed Down Axis", to_string(compositeData->attitude.LinAccelNed.value()[2])));
                }
                if (compositeData->attitude.YprU.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("YprU Yaw", to_string(compositeData->attitude.YprU.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("YprU Pitch", to_string(compositeData->attitude.YprU.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("YprU Roll", to_string(compositeData->attitude.YprU.value()[2])));
                }
                if (compositeData->attitude.Heave.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Heave X-axis", to_string(compositeData->attitude.Heave.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Heave Y-axis", to_string(compositeData->attitude.Heave.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Heave Z-axis", to_string(compositeData->attitude.Heave.value()[2])));
                }
                if (compositeData->attitude.AttU.has_value()) { asciimeasurementsFound.push_back(std::make_pair("AttU", to_string(compositeData->attitude.AttU.value()))); }

                if (compositeData->ins.InsStatus.has_value()) { asciimeasurementsFound.push_back(std::make_pair("InsStatus", to_string(compositeData->ins.InsStatus.value()))); }
                if (compositeData->ins.PosLla.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsPosLla X-axis", to_string(compositeData->ins.PosLla.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsPosLla Y-axis", to_string(compositeData->ins.PosLla.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsPosLla Z-axis", to_string(compositeData->ins.PosLla.value()[2])));
                }
                if (compositeData->ins.PosEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsPosEcef X-axis", to_string(compositeData->ins.PosEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsPosEcef Y-axis", to_string(compositeData->ins.PosEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsPosEcef Z-axis", to_string(compositeData->ins.PosEcef.value()[2])));
                }
                if (compositeData->ins.VelBody.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsVelBody X-axis", to_string(compositeData->ins.VelBody.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsVelBody Y-axis", to_string(compositeData->ins.VelBody.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsVelBody Z-axis", to_string(compositeData->ins.VelBody.value()[2])));
                }
                if (compositeData->ins.VelNed.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsVelNed North axis", to_string(compositeData->ins.VelNed.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsVelNed East axis", to_string(compositeData->ins.VelNed.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsVelNed Down axis", to_string(compositeData->ins.VelNed.value()[2])));
                }
                if (compositeData->ins.VelEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsVelEcef X-axis", to_string(compositeData->ins.VelEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsVelEcef Y-axis", to_string(compositeData->ins.VelEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsVelEcef Z-axis", to_string(compositeData->ins.VelEcef.value()[2])));
                }
                if (compositeData->ins.MagEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsMagEcef X-axis", to_string(compositeData->ins.MagEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsMagEcef Y-axis", to_string(compositeData->ins.MagEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsMagEcef Z-axis", to_string(compositeData->ins.MagEcef.value()[2])));
                }
                if (compositeData->ins.AccelEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsAccelEcef X-axis", to_string(compositeData->ins.AccelEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsAccelEcef Y-axis", to_string(compositeData->ins.AccelEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsAccelEcef Z-axis", to_string(compositeData->ins.AccelEcef.value()[2])));
                }
                if (compositeData->ins.LinAccelEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("InsLinAccelEcef X-axis", to_string(compositeData->ins.LinAccelEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("InsLinAccelEcef Y-axis", to_string(compositeData->ins.LinAccelEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("InsLinAccelEcef Z-axis", to_string(compositeData->ins.LinAccelEcef.value()[2])));
                }
                if (compositeData->ins.PosU.has_value()) { asciimeasurementsFound.push_back(std::make_pair("InsPosU", to_string(compositeData->ins.PosU.value()))); }
                if (compositeData->ins.VelU.has_value()) { asciimeasurementsFound.push_back(std::make_pair("InsVelU", to_string(compositeData->ins.VelU.value()))); }

                if (compositeData->gnss2.TimeUtc.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2TimeUtc (Year-Month-Day-Hour-Min-Sec-Ms)", to_string(2000 + compositeData->gnss2.TimeUtc.value().Year) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Month) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Day) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Hour) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Minute) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Second) + "-" + to_string(compositeData->gnss2.TimeUtc.value().FracSec))); }
                if (compositeData->gnss2.GpsTow.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2GpsTow", to_string(compositeData->gnss2.GpsTow.value()))); }
                if (compositeData->gnss2.GpsWeek.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2GpsWeek", to_string(compositeData->gnss2.GpsWeek.value()))); }
                if (compositeData->gnss2.NumSats.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2NumSats", to_string(compositeData->gnss2.NumSats.value()))); }
                if (compositeData->gnss2.GnssFix.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2Fix", to_string(compositeData->gnss2.GnssFix.value()))); }
                if (compositeData->gnss2.GnssPosLla.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosLla Latitude", to_string(compositeData->gnss2.GnssPosLla.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosLla Longitude", to_string(compositeData->gnss2.GnssPosLla.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosLla Altitude", to_string(compositeData->gnss2.GnssPosLla.value()[2])));
                }
                if (compositeData->gnss2.GnssPosEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosEcef pos[0]", to_string(compositeData->gnss2.GnssPosEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosEcef pos[1]", to_string(compositeData->gnss2.GnssPosEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosEcef pos[2]", to_string(compositeData->gnss2.GnssPosEcef.value()[2])));
                }
                if (compositeData->gnss2.GnssVelNed.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2VelNed North Axis", to_string(compositeData->gnss2.GnssVelNed.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2VelNed East Axis", to_string(compositeData->gnss2.GnssVelNed.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2VelNed Down Axis", to_string(compositeData->gnss2.GnssVelNed.value()[2])));
                }
                if (compositeData->gnss2.GnssVelEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2VelEcef X-Axis", to_string(compositeData->gnss2.GnssVelEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2VelEcef Y-Axis", to_string(compositeData->gnss2.GnssVelEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2VelEcef Z-Axis", to_string(compositeData->gnss2.GnssVelEcef.value()[2])));
                }
                if (compositeData->gnss2.GnssPosUncertainty.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosUncertainty North Axis", to_string(compositeData->gnss2.GnssPosUncertainty.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosUncertainty East Axis", to_string(compositeData->gnss2.GnssPosUncertainty.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2PosUncertainty Down Axis", to_string(compositeData->gnss2.GnssPosUncertainty.value()[2])));

                }
                if (compositeData->gnss2.GnssVelUncertainty.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2VelUncertainty", to_string(compositeData->gnss2.GnssVelUncertainty.value()))); }
                if (compositeData->gnss2.GnssTimeUncertainty.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2TimeUncertainty", to_string(compositeData->gnss2.GnssTimeUncertainty.value()))); }
                if (compositeData->gnss2.GnssTimeInfo.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2TimeInfo Status", to_string(compositeData->gnss2.GnssTimeInfo.value().GnssTimeStatus)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2TimeInfo Leap Seconds", to_string(compositeData->gnss2.GnssTimeInfo.value().LeapSeconds)));
                }
                if (compositeData->gnss2.GnssDop.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2Dop gDOP", to_string(compositeData->gnss2.GnssDop.value().Gdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2Dop pDOP", to_string(compositeData->gnss2.GnssDop.value().Pdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2Dop tDOP", to_string(compositeData->gnss2.GnssDop.value().Tdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2Dop vDOP", to_string(compositeData->gnss2.GnssDop.value().Vdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2Dop hDOP", to_string(compositeData->gnss2.GnssDop.value().Hdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2Dop nDOP", to_string(compositeData->gnss2.GnssDop.value().Ndop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2Dop eDOP", to_string(compositeData->gnss2.GnssDop.value().Edop)));
                }
                if (compositeData->gnss2.GnssSatInfo.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2SatInfo GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Sys.begin(), compositeData->gnss2.GnssSatInfo.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Space Vehicle ID", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().SvId.begin(), compositeData->gnss2.GnssSatInfo.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Flags", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Flags.begin(), compositeData->gnss2.GnssSatInfo.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Cno.begin(), compositeData->gnss2.GnssSatInfo.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Quality Indicator", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Qi.begin(), compositeData->gnss2.GnssSatInfo.value().Qi.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Elevation", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().El.begin(), compositeData->gnss2.GnssSatInfo.value().El.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Azimuth", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Az.begin(), compositeData->gnss2.GnssSatInfo.value().Az.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                }
                if (compositeData->gnss2.GnssRawMeas.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Sys.begin(), compositeData->gnss2.GnssRawMeas.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Space Vehicle ID", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().SvId.begin(), compositeData->gnss2.GnssRawMeas.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Frequency", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Freq.begin(), compositeData->gnss2.GnssRawMeas.value().Freq.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Channel", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Chan.begin(), compositeData->gnss2.GnssRawMeas.value().Chan.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Slot ID", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Slot.begin(), compositeData->gnss2.GnssRawMeas.value().Slot.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Cno.begin(), compositeData->gnss2.GnssRawMeas.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Flags", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Flags.begin(), compositeData->gnss2.GnssRawMeas.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Pseudorange", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Pr.begin(), compositeData->gnss2.GnssRawMeas.value().Pr.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Carrier Phase", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Cp.begin(), compositeData->gnss2.GnssRawMeas.value().Cp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Doppler Measurement", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Dp.begin(), compositeData->gnss2.GnssRawMeas.value().Dp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                }
                if (compositeData->gnss2.GnssStatus.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2Status", to_string(compositeData->gnss2.GnssStatus.value()))); }
                if (compositeData->gnss2.GnssAltMSL.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss2AltMSL", to_string(compositeData->gnss2.GnssAltMSL.value()))); }

                if (compositeData->gnss3.TimeUtc.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3TimeUtc (Year-Month-Day-Hour-Min-Sec-Ms)", to_string(2000 + compositeData->gnss3.TimeUtc.value().Year) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Month) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Day) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Hour) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Minute) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Second) + "-" + to_string(compositeData->gnss3.TimeUtc.value().FracSec))); }
                if (compositeData->gnss3.GpsTow.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3GpsTow", to_string(compositeData->gnss3.GpsTow.value()))); }
                if (compositeData->gnss3.GpsWeek.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3GpsWeek", to_string(compositeData->gnss3.GpsWeek.value()))); }
                if (compositeData->gnss3.NumSats.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3NumSats", to_string(compositeData->gnss3.NumSats.value()))); }
                if (compositeData->gnss3.GnssFix.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3Fix", to_string(compositeData->gnss3.GnssFix.value()))); }
                if (compositeData->gnss3.GnssPosLla.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosLla Latitude", to_string(compositeData->gnss3.GnssPosLla.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosLla Longitude", to_string(compositeData->gnss3.GnssPosLla.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosLla Altitude", to_string(compositeData->gnss3.GnssPosLla.value()[2])));
                }
                if (compositeData->gnss3.GnssPosEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosEcef pos[0]", to_string(compositeData->gnss3.GnssPosEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosEcef pos[1]", to_string(compositeData->gnss3.GnssPosEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosEcef pos[2]", to_string(compositeData->gnss3.GnssPosEcef.value()[2])));
                }
                if (compositeData->gnss3.GnssVelNed.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3VelNed North Axis", to_string(compositeData->gnss3.GnssVelNed.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3VelNed East Axis", to_string(compositeData->gnss3.GnssVelNed.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3VelNed Down Axis", to_string(compositeData->gnss3.GnssVelNed.value()[2])));
                }
                if (compositeData->gnss3.GnssVelEcef.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3VelEcef X-Axis", to_string(compositeData->gnss3.GnssVelEcef.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3VelEcef Y-Axis", to_string(compositeData->gnss3.GnssVelEcef.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3VelEcef Z-Axis", to_string(compositeData->gnss3.GnssVelEcef.value()[2])));
                }
                if (compositeData->gnss3.GnssPosUncertainty.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosUncertainty North Axis", to_string(compositeData->gnss3.GnssPosUncertainty.value()[0])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosUncertainty East Axis", to_string(compositeData->gnss3.GnssPosUncertainty.value()[1])));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3PosUncertainty Down Axis", to_string(compositeData->gnss3.GnssPosUncertainty.value()[2])));
                }
                if (compositeData->gnss3.GnssVelUncertainty.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3VelUncertainty", to_string(compositeData->gnss3.GnssVelUncertainty.value()))); break; }
                if (compositeData->gnss3.GnssTimeUncertainty.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3TimeUncertainty", to_string(compositeData->gnss3.GnssTimeUncertainty.value()))); break; }
                if (compositeData->gnss3.GnssTimeInfo.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3TimeInfo Status", to_string(compositeData->gnss3.GnssTimeInfo.value().GnssTimeStatus)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3TimeInfo Leap Seconds", to_string(compositeData->gnss3.GnssTimeInfo.value().LeapSeconds)));
                }
                if (compositeData->gnss3.GnssDop.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3Dop gDOP", to_string(compositeData->gnss3.GnssDop.value().Gdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3Dop pDOP", to_string(compositeData->gnss3.GnssDop.value().Pdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3Dop tDOP", to_string(compositeData->gnss3.GnssDop.value().Tdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3Dop vDOP", to_string(compositeData->gnss3.GnssDop.value().Vdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3Dop hDOP", to_string(compositeData->gnss3.GnssDop.value().Hdop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3Dop nDOP", to_string(compositeData->gnss3.GnssDop.value().Ndop)));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3Dop eDOP", to_string(compositeData->gnss3.GnssDop.value().Edop)));
                }
                if (compositeData->gnss3.GnssSatInfo.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3SatInfo GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Sys.begin(), compositeData->gnss3.GnssSatInfo.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Space Vehicle ID", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().SvId.begin(), compositeData->gnss3.GnssSatInfo.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Flags", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Flags.begin(), compositeData->gnss3.GnssSatInfo.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Cno.begin(), compositeData->gnss3.GnssSatInfo.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Quality Indicator", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Qi.begin(), compositeData->gnss3.GnssSatInfo.value().Qi.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Elevation", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().El.begin(), compositeData->gnss3.GnssSatInfo.value().El.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Azimuth", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Az.begin(), compositeData->gnss3.GnssSatInfo.value().Az.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                }
                if (compositeData->gnss3.GnssRawMeas.has_value()) {
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Sys.begin(), compositeData->gnss3.GnssRawMeas.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Space Vehicle ID", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().SvId.begin(), compositeData->gnss3.GnssRawMeas.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Frequency", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Freq.begin(), compositeData->gnss3.GnssRawMeas.value().Freq.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Channel", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Chan.begin(), compositeData->gnss3.GnssRawMeas.value().Chan.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Slot ID", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Slot.begin(), compositeData->gnss3.GnssRawMeas.value().Slot.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Cno.begin(), compositeData->gnss3.GnssRawMeas.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Flags", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Flags.begin(), compositeData->gnss3.GnssRawMeas.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Pseudorange", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Pr.begin(), compositeData->gnss3.GnssRawMeas.value().Pr.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Carrier Phase", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Cp.begin(), compositeData->gnss3.GnssRawMeas.value().Cp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                    asciimeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Doppler Measurement", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Dp.begin(), compositeData->gnss3.GnssRawMeas.value().Dp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                }
                if (compositeData->gnss3.GnssStatus.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3Status", to_string(compositeData->gnss3.GnssStatus.value()))); }
                if (compositeData->gnss3.GnssAltMSL.has_value()) { asciimeasurementsFound.push_back(std::make_pair("Gnss3AltMSL", to_string(compositeData->gnss3.GnssAltMSL.value()))); }
            }
            else {
                std::cerr << "Not a valid packet!" << std::endl;
            }
        }
    }

    void binaryParser(std::vector<std::string> binaryData) {
        for (const auto& line : binaryData) {
            VN::ByteBuffer currentByteStream(line.size());
            std::vector<uint8_t> dataBytes = rawbytesinsertor(line);
            currentByteStream.put(dataBytes.data(), dataBytes.size());
            auto findPacketResult = VN::FaPacketProtocol::findPacket(currentByteStream, 0);

            if (findPacketResult.validity == VN::FaPacketProtocol::Validity::Valid) {
                auto packetMetadata = findPacketResult.metadata;
                auto compositeData = VN::FaPacketProtocol::parsePacket(currentByteStream, 0, packetMetadata.header, packetMetadata.header.toMeasurementHeader());


                if (compositeData.has_value()) {
                    for (uint8_t measGroupIndex : packetMetadata.header.outputGroups) {
                        for (uint8_t measTypeIndex : packetMetadata.header.outputTypes) {
                            void* ptr = compositeData.value().getMeasurementType(measGroupIndex, measTypeIndex);
                            switch (measGroupIndex) {
                            case 1:
                                switch (measTypeIndex) {
                                case 1:
                                    binmeasurementsFound.push_back(std::make_pair("TimeStartup", to_string(compositeData->time.TimeStartup.value()))); 
                                    break;
                                case 2:
                                    binmeasurementsFound.push_back(std::make_pair("TimeGPS", to_string(compositeData->time.TimeGps.value()))); break;
                                case 3:
                                    binmeasurementsFound.push_back(std::make_pair("GpsTow", to_string(compositeData->time.GpsTow.value()))); break;
                                case 4:
                                    binmeasurementsFound.push_back(std::make_pair("GpsWeek", to_string(compositeData->time.GpsWeek.value()))); break;
                                case 5:
                                    binmeasurementsFound.push_back(std::make_pair("TimeSyncIn", to_string(compositeData->time.TimeSyncIn.value()))); break;
                                case 6:
                                    binmeasurementsFound.push_back(std::make_pair("TimeGpsPps", to_string(compositeData->time.TimeGpsPps.value()))); break;
                                case 7:
                                    binmeasurementsFound.push_back(std::make_pair("TimeUtc", to_string(2000 + compositeData->time.TimeUtc.value().Year) + "-" + to_string(compositeData->time.TimeUtc.value().Month) + "-" + to_string(compositeData->time.TimeUtc.value().Day) + "-" + to_string(compositeData->time.TimeUtc.value().Hour) + "-" + to_string(compositeData->time.TimeUtc.value().Minute) + "-" + to_string(compositeData->time.TimeUtc.value().Second) + "-" + to_string(compositeData->time.TimeUtc.value().FracSec))); break;
                                case 8:
                                    binmeasurementsFound.push_back(std::make_pair("SyncInCnt", to_string(compositeData->time.SyncInCnt.value()))); break;
                                case 9:
                                    binmeasurementsFound.push_back(std::make_pair("SyncOutCnt", to_string(compositeData->time.SyncOutCnt.value()))); break;
                                case 10:
                                    binmeasurementsFound.push_back(std::make_pair("TimeStatus", to_string(compositeData->time.TimeStatus.value()))); break;
                                }
                                break;

                            case 2:
                                switch (measTypeIndex) {
                                case 1:
                                    binmeasurementsFound.push_back(std::make_pair("ImuStatus", to_string(compositeData->imu.ImuStatus.value()))); break;
                                case 2:
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompMag X-axis", to_string(compositeData->imu.UncompMag.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompMag Y-axis", to_string(compositeData->imu.UncompMag.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompMag Z-axis", to_string(compositeData->imu.UncompMag.value()[2])));
                                    break;
                                case 3:
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompAccel X-axis", to_string(compositeData->imu.UncompAccel.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompAccel Y-axis", to_string(compositeData->imu.UncompAccel.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompAccel Z-axis", to_string(compositeData->imu.UncompAccel.value()[2])));
                                    break;
                                case 4:
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompGyro X-axis", to_string(compositeData->imu.UncompGyro.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompGyro Y-axis", to_string(compositeData->imu.UncompGyro.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuUncompGyro Z-axis", to_string(compositeData->imu.UncompGyro.value()[2])));
                                    break;
                                case 5:
                                    binmeasurementsFound.push_back(std::make_pair("ImuTemperature", to_string(compositeData->imu.Temperature.value()))); break;
                                case 6:
                                    binmeasurementsFound.push_back(std::make_pair("ImuPressure", to_string(compositeData->imu.Pressure.value()))); break;
                                case 7:
                                    binmeasurementsFound.push_back(std::make_pair("ImuDeltaTheta dtime", to_string(compositeData->imu.DeltaTheta.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuDeltaTheta X-axis", to_string(compositeData->imu.DeltaTheta.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuDeltaTheta Y-axis", to_string(compositeData->imu.DeltaTheta.value()[2])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuDeltaTheta Z-axis", to_string(compositeData->imu.DeltaTheta.value()[3])));
                                    break;
                                case 8:
                                    binmeasurementsFound.push_back(std::make_pair("ImuDeltaVel X-axis", to_string(compositeData->imu.DeltaVel.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuDeltaVel Y-axis", to_string(compositeData->imu.DeltaVel.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuDeltaVel Z-axis", to_string(compositeData->imu.DeltaVel.value()[2])));
                                    break;
                                case 9:
                                    binmeasurementsFound.push_back(std::make_pair("ImuMag X-axis", to_string(compositeData->imu.Mag.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuMag Y-axis", to_string(compositeData->imu.Mag.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuMag Z-axis", to_string(compositeData->imu.Mag.value()[2])));
                                    break;
                                case 10:
                                    binmeasurementsFound.push_back(std::make_pair("ImuAccel X-axis", to_string(compositeData->imu.Accel.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuAccel Y-axis", to_string(compositeData->imu.Accel.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuAccel Z-axis", to_string(compositeData->imu.Accel.value()[2])));
                                    break;
                                case 11:
                                    binmeasurementsFound.push_back(std::make_pair("ImuAngularRate X-axis", to_string(compositeData->imu.AngularRate.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuAngularRate Y-axis", to_string(compositeData->imu.AngularRate.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("ImuAngularRate Z-axis", to_string(compositeData->imu.AngularRate.value()[2])));
                                    break;
                                case 12:
                                    binmeasurementsFound.push_back(std::make_pair("SensSat", to_string(compositeData->imu.SensSat.value()))); break;
                                }
                                break;
                            case 3:
                                switch (measTypeIndex) {
                                case 1:
                                    binmeasurementsFound.push_back(std::make_pair("GnssTimeUtc (Year-Month-Day-Hour-Min-Sec-Ms)", to_string(2000 + compositeData->gnss.TimeUtc.value().Year) + "-" + to_string(compositeData->gnss.TimeUtc.value().Month) + "-" + to_string(compositeData->gnss.TimeUtc.value().Day) + "-" + to_string(compositeData->gnss.TimeUtc.value().Hour) + "-" + to_string(compositeData->gnss.TimeUtc.value().Minute) + "-" + to_string(compositeData->gnss.TimeUtc.value().Second) + "-" + to_string(compositeData->gnss.TimeUtc.value().FracSec))); break;
                                case 2:
                                    binmeasurementsFound.push_back(std::make_pair("GnssGpsTow", to_string(compositeData->gnss.GpsTow.value()))); break;
                                case 3:
                                    binmeasurementsFound.push_back(std::make_pair("GnssGpsWeek", to_string(compositeData->gnss.GpsWeek.value()))); break;
                                case 4:
                                    binmeasurementsFound.push_back(std::make_pair("GnssNumSats", to_string(compositeData->gnss.NumSats.value()))); break;
                                case 5:
                                    binmeasurementsFound.push_back(std::make_pair("GnssFix", to_string(compositeData->gnss.GnssFix.value()))); break;
                                case 6:
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosLla Latitude", to_string(compositeData->gnss.GnssPosLla.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosLla Longitude", to_string(compositeData->gnss.GnssPosLla.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosLla Altitude", to_string(compositeData->gnss.GnssPosLla.value()[2])));
                                    break;
                                case 7:
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosEcef pos[0]", to_string(compositeData->gnss.GnssPosEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosEcef pos[1]", to_string(compositeData->gnss.GnssPosEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosEcef pos[2]", to_string(compositeData->gnss.GnssPosEcef.value()[2])));
                                    break;
                                case 8:
                                    binmeasurementsFound.push_back(std::make_pair("GnssVelNed North Axis", to_string(compositeData->gnss.GnssVelNed.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssVelNed East Axis", to_string(compositeData->gnss.GnssVelNed.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssVelNed Down Axis", to_string(compositeData->gnss.GnssVelNed.value()[2])));
                                    break;
                                case 9:
                                    binmeasurementsFound.push_back(std::make_pair("GnssVelEcef X-Axis", to_string(compositeData->gnss.GnssVelEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssVelEcef Y-Axis", to_string(compositeData->gnss.GnssVelEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssVelEcef Z-Axis", to_string(compositeData->gnss.GnssVelEcef.value()[2])));
                                    break;
                                case 10:
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosUncertainty North Axis", to_string(compositeData->gnss.GnssPosUncertainty.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosUncertainty East Axis", to_string(compositeData->gnss.GnssPosUncertainty.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("GnssPosUncertainty Down Axis", to_string(compositeData->gnss.GnssPosUncertainty.value()[2])));
                                    break;
                                case 11:
                                    binmeasurementsFound.push_back(std::make_pair("GnssVelUncertainty", to_string(compositeData->gnss.GnssVelUncertainty.value()))); break;
                                case 12:
                                    binmeasurementsFound.push_back(std::make_pair("GnssTimeUncertainty", to_string(compositeData->gnss.GnssTimeUncertainty.value()))); break;
                                case 13:
                                    binmeasurementsFound.push_back(std::make_pair("GnssTimeInfo Status", to_string(compositeData->gnss.GnssTimeInfo.value().GnssTimeStatus)));
                                    binmeasurementsFound.push_back(std::make_pair("GnssTimeInfo Leap Seconds", to_string(compositeData->gnss.GnssTimeInfo.value().LeapSeconds)));
                                    break;
                                case 14:
                                    binmeasurementsFound.push_back(std::make_pair("GnssDop gDOP", to_string(compositeData->gnss.GnssDop.value().Gdop)));
                                    binmeasurementsFound.push_back(std::make_pair("GnssDop pDOP", to_string(compositeData->gnss.GnssDop.value().Pdop)));
                                    binmeasurementsFound.push_back(std::make_pair("GnssDop tDOP", to_string(compositeData->gnss.GnssDop.value().Tdop)));
                                    binmeasurementsFound.push_back(std::make_pair("GnssDop vDOP", to_string(compositeData->gnss.GnssDop.value().Vdop)));
                                    binmeasurementsFound.push_back(std::make_pair("GnssDop hDOP", to_string(compositeData->gnss.GnssDop.value().Hdop)));
                                    binmeasurementsFound.push_back(std::make_pair("GnssDop nDOP", to_string(compositeData->gnss.GnssDop.value().Ndop)));
                                    binmeasurementsFound.push_back(std::make_pair("GnssDop eDOP", to_string(compositeData->gnss.GnssDop.value().Edop)));
                                    break;
                                case 15:
                                    binmeasurementsFound.push_back(std::make_pair("GnssSatInfo GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Sys.begin(), compositeData->gnss.GnssSatInfo.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssSatInfo Space Vehicle ID", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().SvId.begin(), compositeData->gnss.GnssSatInfo.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssSatInfo Flags", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Flags.begin(), compositeData->gnss.GnssSatInfo.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssSatInfo Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Cno.begin(), compositeData->gnss.GnssSatInfo.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssSatInfo Quality Indicator", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Qi.begin(), compositeData->gnss.GnssSatInfo.value().Qi.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssSatInfo Elevation", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().El.begin(), compositeData->gnss.GnssSatInfo.value().El.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssSatInfo Azimuth", "[" + std::accumulate(compositeData->gnss.GnssSatInfo.value().Az.begin(), compositeData->gnss.GnssSatInfo.value().Az.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    break;
                                case 16:
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Sys.begin(), compositeData->gnss.GnssRawMeas.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Space Vehicle ID", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().SvId.begin(), compositeData->gnss.GnssRawMeas.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Frequency", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Freq.begin(), compositeData->gnss.GnssRawMeas.value().Freq.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Channel", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Chan.begin(), compositeData->gnss.GnssRawMeas.value().Chan.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Slot ID", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Slot.begin(), compositeData->gnss.GnssRawMeas.value().Slot.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Cno.begin(), compositeData->gnss.GnssRawMeas.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Flags", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Flags.begin(), compositeData->gnss.GnssRawMeas.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Pseudorange", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Pr.begin(), compositeData->gnss.GnssRawMeas.value().Pr.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Carrier Phase", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Cp.begin(), compositeData->gnss.GnssRawMeas.value().Cp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("GnssRawMeas Doppler Measurement", "[" + std::accumulate(compositeData->gnss.GnssRawMeas.value().Dp.begin(), compositeData->gnss.GnssRawMeas.value().Dp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    break;
                                case 17:
                                    binmeasurementsFound.push_back(std::make_pair("GnssStatus", to_string(compositeData->gnss.GnssStatus.value()))); break;
                                case 18:
                                    binmeasurementsFound.push_back(std::make_pair("GnssAltMSL", to_string(compositeData->gnss.GnssAltMSL.value()))); break;
                                }
                                break;
                            case 4:
                                switch (measTypeIndex) {
                                case 1:
                                    binmeasurementsFound.push_back(std::make_pair("Ypr Yaw", to_string(compositeData->attitude.Ypr.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Ypr Pitch", to_string(compositeData->attitude.Ypr.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Ypr Roll", to_string(compositeData->attitude.Ypr.value()[2])));
                                    break;
                                case 2:
                                    binmeasurementsFound.push_back(std::make_pair("Quaternion qtn[0]", to_string(compositeData->attitude.Quaternion.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Quaternion qtn[1]", to_string(compositeData->attitude.Quaternion.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Quaternion qtn[2]", to_string(compositeData->attitude.Quaternion.value()[2])));
                                    binmeasurementsFound.push_back(std::make_pair("Quaternion qtn[3]", to_string(compositeData->attitude.Quaternion.value()[2])));
                                    break;
                                case 3:
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[0]", to_string(compositeData->attitude.Quaternion.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[1]", to_string(compositeData->attitude.Quaternion.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[2]", to_string(compositeData->attitude.Quaternion.value()[2])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[3]", to_string(compositeData->attitude.Quaternion.value()[3])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[4]", to_string(compositeData->attitude.Quaternion.value()[4])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[5]", to_string(compositeData->attitude.Quaternion.value()[5])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[6]", to_string(compositeData->attitude.Quaternion.value()[6])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[7]", to_string(compositeData->attitude.Quaternion.value()[7])));
                                    binmeasurementsFound.push_back(std::make_pair("Dcm dcm[8]", to_string(compositeData->attitude.Quaternion.value()[8])));
                                    break;
                                case 4:
                                    binmeasurementsFound.push_back(std::make_pair("MagNed North Axis", to_string(compositeData->attitude.MagNed.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("MagNed East Axis", to_string(compositeData->attitude.MagNed.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("MagNed Down Axis", to_string(compositeData->attitude.MagNed.value()[2])));
                                    break;
                                case 5:
                                    binmeasurementsFound.push_back(std::make_pair("AccelNed North Axis", to_string(compositeData->attitude.AccelNed.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("AccelNed East Axis", to_string(compositeData->attitude.AccelNed.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("AccelNed Down Axis", to_string(compositeData->attitude.AccelNed.value()[2])));
                                    break;
                                case 6:
                                    binmeasurementsFound.push_back(std::make_pair("LinBodyAcc X-Axis", to_string(compositeData->attitude.LinBodyAcc.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("LinBodyAcc Y-Axis", to_string(compositeData->attitude.LinBodyAcc.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("LinBodyAcc Z-Axis", to_string(compositeData->attitude.LinBodyAcc.value()[2])));
                                    break;
                                case 7:
                                    binmeasurementsFound.push_back(std::make_pair("LinAccelNed North Axis", to_string(compositeData->attitude.LinAccelNed.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("LinAccelNed East Axis", to_string(compositeData->attitude.LinAccelNed.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("LinAccelNed Down Axis", to_string(compositeData->attitude.LinAccelNed.value()[2])));
                                    break;
                                case 8:
                                    binmeasurementsFound.push_back(std::make_pair("YprU Yaw", to_string(compositeData->attitude.YprU.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("YprU Pitch", to_string(compositeData->attitude.YprU.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("YprU Roll", to_string(compositeData->attitude.YprU.value()[2])));
                                    break;
                                case 12:
                                    binmeasurementsFound.push_back(std::make_pair("Heave X-axis", to_string(compositeData->attitude.Heave.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Heave Y-axis", to_string(compositeData->attitude.Heave.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Heave Z-axis", to_string(compositeData->attitude.Heave.value()[2])));
                                    break;
                                case 13:
                                    binmeasurementsFound.push_back(std::make_pair("AttU", to_string(compositeData->attitude.AttU.value()))); break;
                                }
                                break;
                            case 5:
                                switch (measTypeIndex) {
                                case 1:
                                    binmeasurementsFound.push_back(std::make_pair("InsStatus", to_string(compositeData->ins.InsStatus.value()))); break;
                                case 2:
                                    binmeasurementsFound.push_back(std::make_pair("InsPosLla X-axis", to_string(compositeData->ins.PosLla.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsPosLla Y-axis", to_string(compositeData->ins.PosLla.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsPosLla Z-axis", to_string(compositeData->ins.PosLla.value()[2])));
                                    break;
                                case 3:
                                    binmeasurementsFound.push_back(std::make_pair("InsPosEcef X-axis", to_string(compositeData->ins.PosEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsPosEcef Y-axis", to_string(compositeData->ins.PosEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsPosEcef Z-axis", to_string(compositeData->ins.PosEcef.value()[2])));
                                    break;
                                case 4:
                                    binmeasurementsFound.push_back(std::make_pair("InsVelBody X-axis", to_string(compositeData->ins.VelBody.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsVelBody Y-axis", to_string(compositeData->ins.VelBody.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsVelBody Z-axis", to_string(compositeData->ins.VelBody.value()[2])));
                                    break;
                                case 5:
                                    binmeasurementsFound.push_back(std::make_pair("InsVelNed North axis", to_string(compositeData->ins.VelNed.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsVelNed East axis", to_string(compositeData->ins.VelNed.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsVelNed Down axis", to_string(compositeData->ins.VelNed.value()[2])));
                                    break;
                                case 6:
                                    binmeasurementsFound.push_back(std::make_pair("InsVelEcef X-axis", to_string(compositeData->ins.VelEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsVelEcef Y-axis", to_string(compositeData->ins.VelEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsVelEcef Z-axis", to_string(compositeData->ins.VelEcef.value()[2])));
                                    break;
                                case 7:
                                    binmeasurementsFound.push_back(std::make_pair("InsMagEcef X-axis", to_string(compositeData->ins.MagEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsMagEcef Y-axis", to_string(compositeData->ins.MagEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsMagEcef Z-axis", to_string(compositeData->ins.MagEcef.value()[2])));
                                    break;
                                case 8:
                                    binmeasurementsFound.push_back(std::make_pair("InsAccelEcef X-axis", to_string(compositeData->ins.AccelEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsAccelEcef Y-axis", to_string(compositeData->ins.AccelEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsAccelEcef Z-axis", to_string(compositeData->ins.AccelEcef.value()[2])));
                                    break;
                                case 9:
                                    binmeasurementsFound.push_back(std::make_pair("InsLinAccelEcef X-axis", to_string(compositeData->ins.LinAccelEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("InsLinAccelEcef Y-axis", to_string(compositeData->ins.LinAccelEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("InsLinAccelEcef Z-axis", to_string(compositeData->ins.LinAccelEcef.value()[2])));
                                    break;
                                case 10:
                                    binmeasurementsFound.push_back(std::make_pair("InsPosU", to_string(compositeData->ins.PosU.value()))); break;
                                case 11:
                                    binmeasurementsFound.push_back(std::make_pair("InsVelU", to_string(compositeData->ins.VelU.value()))); break;
                                }
                                break;
                            case 6:
                                switch (measTypeIndex) {
                                case 1:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2TimeUtc (Year-Month-Day-Hour-Min-Sec-Ms)", to_string(2000 + compositeData->gnss2.TimeUtc.value().Year) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Month) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Day) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Hour) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Minute) + "-" + to_string(compositeData->gnss2.TimeUtc.value().Second) + "-" + to_string(compositeData->gnss2.TimeUtc.value().FracSec))); break;
                                case 2:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2GpsTow", to_string(compositeData->gnss2.GpsTow.value()))); break;
                                case 3:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2GpsWeek", to_string(compositeData->gnss2.GpsWeek.value()))); break;
                                case 4:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2NumSats", to_string(compositeData->gnss2.NumSats.value()))); break;
                                case 5:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Fix", to_string(compositeData->gnss2.GnssFix.value()))); break;
                                case 6:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosLla Latitude", to_string(compositeData->gnss2.GnssPosLla.value()[0]))); 
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosLla Longitude", to_string(compositeData->gnss2.GnssPosLla.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosLla Altitude", to_string(compositeData->gnss2.GnssPosLla.value()[2])));
                                    break;
                                case 7:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosEcef pos[0]", to_string(compositeData->gnss2.GnssPosEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosEcef pos[1]", to_string(compositeData->gnss2.GnssPosEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosEcef pos[2]", to_string(compositeData->gnss2.GnssPosEcef.value()[2])));
                                    break;                                
                                case 8:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2VelNed North Axis", to_string(compositeData->gnss2.GnssVelNed.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2VelNed East Axis", to_string(compositeData->gnss2.GnssVelNed.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2VelNed Down Axis", to_string(compositeData->gnss2.GnssVelNed.value()[2])));
                                    break;                                  
                                case 9:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2VelEcef X-Axis", to_string(compositeData->gnss2.GnssVelEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2VelEcef Y-Axis", to_string(compositeData->gnss2.GnssVelEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2VelEcef Z-Axis", to_string(compositeData->gnss2.GnssVelEcef.value()[2])));
                                    break;
                                case 10:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosUncertainty North Axis", to_string(compositeData->gnss2.GnssPosUncertainty.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosUncertainty East Axis", to_string(compositeData->gnss2.GnssPosUncertainty.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2PosUncertainty Down Axis", to_string(compositeData->gnss2.GnssPosUncertainty.value()[2])));
                                    break;
                                case 11:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2VelUncertainty", to_string(compositeData->gnss2.GnssVelUncertainty.value()))); break;
                                case 12:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2TimeUncertainty", to_string(compositeData->gnss2.GnssTimeUncertainty.value()))); break;
                                case 13:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2TimeInfo Status", to_string(compositeData->gnss2.GnssTimeInfo.value().GnssTimeStatus)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2TimeInfo Leap Seconds", to_string(compositeData->gnss2.GnssTimeInfo.value().LeapSeconds)));
                                    break; 
                                case 14:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Dop gDOP", to_string(compositeData->gnss2.GnssDop.value().Gdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Dop pDOP", to_string(compositeData->gnss2.GnssDop.value().Pdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Dop tDOP", to_string(compositeData->gnss2.GnssDop.value().Tdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Dop vDOP", to_string(compositeData->gnss2.GnssDop.value().Vdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Dop hDOP", to_string(compositeData->gnss2.GnssDop.value().Hdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Dop nDOP", to_string(compositeData->gnss2.GnssDop.value().Ndop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Dop eDOP", to_string(compositeData->gnss2.GnssDop.value().Edop)));
                                    break; 
                                case 15:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2SatInfo GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Sys.begin(), compositeData->gnss2.GnssSatInfo.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Space Vehicle ID", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().SvId.begin(), compositeData->gnss2.GnssSatInfo.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Flags", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Flags.begin(), compositeData->gnss2.GnssSatInfo.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Cno.begin(), compositeData->gnss2.GnssSatInfo.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Quality Indicator", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Qi.begin(), compositeData->gnss2.GnssSatInfo.value().Qi.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Elevation", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().El.begin(), compositeData->gnss2.GnssSatInfo.value().El.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2SatInfo Azimuth", "[" + std::accumulate(compositeData->gnss2.GnssSatInfo.value().Az.begin(), compositeData->gnss2.GnssSatInfo.value().Az.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    break;
                                case 16:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Sys.begin(), compositeData->gnss2.GnssRawMeas.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Space Vehicle ID", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().SvId.begin(), compositeData->gnss2.GnssRawMeas.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Frequency", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Freq.begin(), compositeData->gnss2.GnssRawMeas.value().Freq.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Channel", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Chan.begin(), compositeData->gnss2.GnssRawMeas.value().Chan.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Slot ID", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Slot.begin(), compositeData->gnss2.GnssRawMeas.value().Slot.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Cno.begin(), compositeData->gnss2.GnssRawMeas.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Flags", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Flags.begin(), compositeData->gnss2.GnssRawMeas.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Pseudorange", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Pr.begin(), compositeData->gnss2.GnssRawMeas.value().Pr.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Carrier Phase", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Cp.begin(), compositeData->gnss2.GnssRawMeas.value().Cp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2RawMeas Doppler Measurement", "[" + std::accumulate(compositeData->gnss2.GnssRawMeas.value().Dp.begin(), compositeData->gnss2.GnssRawMeas.value().Dp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    break;
                                case 17:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2Status", to_string(compositeData->gnss2.GnssStatus.value()))); break;
                                case 18:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss2AltMSL", to_string(compositeData->gnss2.GnssAltMSL.value()))); break;
                                }
                                break;
                            case 7:
                                switch (measTypeIndex) {
                                case 1:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3TimeUtc (Year-Month-Day-Hour-Min-Sec-Ms)", to_string(2000 + compositeData->gnss3.TimeUtc.value().Year) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Month) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Day) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Hour) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Minute) + "-" + to_string(compositeData->gnss3.TimeUtc.value().Second) + "-" + to_string(compositeData->gnss3.TimeUtc.value().FracSec))); break;
                                case 2:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3GpsTow", to_string(compositeData->gnss3.GpsTow.value()))); break;
                                case 3:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3GpsWeek", to_string(compositeData->gnss3.GpsWeek.value()))); break;
                                case 4:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3NumSats", to_string(compositeData->gnss3.NumSats.value()))); break;
                                case 5:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Fix", to_string(compositeData->gnss3.GnssFix.value()))); break;
                                case 6:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosLla Latitude", to_string(compositeData->gnss3.GnssPosLla.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosLla Longitude", to_string(compositeData->gnss3.GnssPosLla.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosLla Altitude", to_string(compositeData->gnss3.GnssPosLla.value()[2])));
                                    break;
                                case 7:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosEcef pos[0]", to_string(compositeData->gnss3.GnssPosEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosEcef pos[1]", to_string(compositeData->gnss3.GnssPosEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosEcef pos[2]", to_string(compositeData->gnss3.GnssPosEcef.value()[2])));
                                    break;
                                case 8:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3VelNed North Axis", to_string(compositeData->gnss3.GnssVelNed.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3VelNed East Axis", to_string(compositeData->gnss3.GnssVelNed.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3VelNed Down Axis", to_string(compositeData->gnss3.GnssVelNed.value()[2])));
                                    break;
                                case 9:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3VelEcef X-Axis", to_string(compositeData->gnss3.GnssVelEcef.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3VelEcef Y-Axis", to_string(compositeData->gnss3.GnssVelEcef.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3VelEcef Z-Axis", to_string(compositeData->gnss3.GnssVelEcef.value()[2])));
                                    break;
                                case 10:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosUncertainty North Axis", to_string(compositeData->gnss3.GnssPosUncertainty.value()[0])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosUncertainty East Axis", to_string(compositeData->gnss3.GnssPosUncertainty.value()[1])));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3PosUncertainty Down Axis", to_string(compositeData->gnss3.GnssPosUncertainty.value()[2])));
                                    break;
                                case 11:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3VelUncertainty", to_string(compositeData->gnss3.GnssVelUncertainty.value()))); break;
                                case 12:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3TimeUncertainty", to_string(compositeData->gnss3.GnssTimeUncertainty.value()))); break;
                                case 13:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3TimeInfo Status", to_string(compositeData->gnss3.GnssTimeInfo.value().GnssTimeStatus)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3TimeInfo Leap Seconds", to_string(compositeData->gnss3.GnssTimeInfo.value().LeapSeconds)));
                                    break;
                                case 14:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Dop gDOP", to_string(compositeData->gnss3.GnssDop.value().Gdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Dop pDOP", to_string(compositeData->gnss3.GnssDop.value().Pdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Dop tDOP", to_string(compositeData->gnss3.GnssDop.value().Tdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Dop vDOP", to_string(compositeData->gnss3.GnssDop.value().Vdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Dop hDOP", to_string(compositeData->gnss3.GnssDop.value().Hdop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Dop nDOP", to_string(compositeData->gnss3.GnssDop.value().Ndop)));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Dop eDOP", to_string(compositeData->gnss3.GnssDop.value().Edop)));
                                    break;
                                case 15:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3SatInfo GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Sys.begin(), compositeData->gnss3.GnssSatInfo.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Space Vehicle ID", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().SvId.begin(), compositeData->gnss3.GnssSatInfo.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Flags", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Flags.begin(), compositeData->gnss3.GnssSatInfo.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Cno.begin(), compositeData->gnss3.GnssSatInfo.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Quality Indicator", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Qi.begin(), compositeData->gnss3.GnssSatInfo.value().Qi.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Elevation", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().El.begin(), compositeData->gnss3.GnssSatInfo.value().El.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3SatInfo Azimuth", "[" + std::accumulate(compositeData->gnss3.GnssSatInfo.value().Az.begin(), compositeData->gnss3.GnssSatInfo.value().Az.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    break;
                                case 16:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas GNSS Constellation Indicator", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Sys.begin(), compositeData->gnss3.GnssRawMeas.value().Sys.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Space Vehicle ID", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().SvId.begin(), compositeData->gnss3.GnssRawMeas.value().SvId.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Frequency", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Freq.begin(), compositeData->gnss3.GnssRawMeas.value().Freq.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Channel", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Chan.begin(), compositeData->gnss3.GnssRawMeas.value().Chan.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Slot ID", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Slot.begin(), compositeData->gnss3.GnssRawMeas.value().Slot.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Carrier-to-noise density ratio", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Cno.begin(), compositeData->gnss3.GnssRawMeas.value().Cno.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Flags", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Flags.begin(), compositeData->gnss3.GnssRawMeas.value().Flags.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Pseudorange", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Pr.begin(), compositeData->gnss3.GnssRawMeas.value().Pr.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Carrier Phase", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Cp.begin(), compositeData->gnss3.GnssRawMeas.value().Cp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3RawMeas Doppler Measurement", "[" + std::accumulate(compositeData->gnss3.GnssRawMeas.value().Dp.begin(), compositeData->gnss3.GnssRawMeas.value().Dp.end(), std::string{}, [](const std::string& a, int b) { std::ostringstream ss; ss << b; return a.empty() ? ss.str() : a + ", " + ss.str(); }) + "]"));
                                    break;
                                case 17:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3Status", to_string(compositeData->gnss3.GnssStatus.value()))); break;
                                case 18:
                                    binmeasurementsFound.push_back(std::make_pair("Gnss3AltMSL", to_string(compositeData->gnss3.GnssAltMSL.value()))); break;
                                }
                                break;
                            }
                        }
                    }
                }
                else {
                    std::cerr << "Failed to parse the packet." << std::endl;
                }
            }
            else {
                std::cerr << "Not a valid packet!" << std::endl;
            }
        }
    }


public: 
    Parser() {};
    Parser(std::string filename) : reader(filename, std::ios::binary), filePath(filename) {
        try {
            if (std::filesystem::exists(filePath)) {
                if (std::filesystem::is_directory(filePath)) {
                    // Create Folders
                    std::filesystem::create_directory("\\Converted Logs\\");
                    std::filesystem::create_directory("\\Converted Logs\\ASCII Measurements\\");
                    std::filesystem::create_directory("\\Converted Logs\\Binary Measurements\\");
                    

                    for (const std::filesystem::path& dirEntry : recursive_directory_iterator(filePath)) {
                        if (dirEntry.extension().string() == ".bin") {
                            readFileContents();
                        }
                    }
                }
                else {
                    if (filePath.extension() == ".bin") {
                        std::cout << filePath.parent_path() << std::endl;
                        readFileContents();
                    }
                }
            }
            else {
                throw (false); 
            }
        }
        catch (bool fileExistence) {
            std::cout << "The provided directory does not exist!" << std::endl; 
        }
    };
    void fileConversion(std::string type) {
        if (type == "text") {
            TextConverter tc(filePath, parsedSensorData.ASCIIPrinterFriendly, parsedSensorData.binaryMeasurements);
        }
        else if (type == "csv") {
            asciiParser(parsedSensorData.ASCIIMeasurements);
            binaryParser(parsedSensorData.binaryMeasurements);
            CSVConverter csvc(filePath, asciimeasurementsFound, binmeasurementsFound);
        }/* else if (type == "matlab") {
            // Handle MATLAB case
        }*/
    }
};