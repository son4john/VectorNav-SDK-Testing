#include <fstream>
#include <thread>
#include "TemplateLibrary/ByteBuffer.hpp"

using namespace VN;

class Logging{
    private:
        ByteBuffer* byteBuffer_;
        std::ofstream file_;
        std::thread thread_;

    public:
        Logging(){};
        Logging(ByteBuffer* byteBuffer, const std::string& filename) : byteBuffer_(byteBuffer), file_(filename, std::ios::binary){
            thread_ = std::thread(&Logging::logBytes, this);
        }

        void logBytes(){
            size_t numBytesLog = byteBuffer_->size();
            const int LOG_BUFFER_CAPACITY = 5000;
            std::array<uint8_t,LOG_BUFFER_CAPACITY> logBuffer{};

            byteBuffer_->peek(logBuffer.data(), numBytesLog);
            file_.write(reinterpret_cast<const char*>(logBuffer.data()), numBytesLog);
            byteBuffer_->discardBytes(numBytesLog);            
        }

        void closeLog(){
            file_.close();
        }

        ~Logging() {
            thread_.join();
        }

};