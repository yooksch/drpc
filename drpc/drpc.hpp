/*
MIT License

Copyright 2025 yooksch

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files (the “Software”),
to deal in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef DRPC_HPP
#define DRPC_HPP

#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <random>
#include <vector>
#include <string>
#include <array>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace DiscordRichPresence {
    namespace UUID {
        inline std::string GenerateUUIDv4() {
            static std::random_device rd;
            static std::mt19937 mt(rd());
            static std::uniform_int_distribution<int> distribution(0, 15);
            const auto get_digit = [] {
                int value = distribution(mt);
                if (value < 10) return '0' + value;
                return 'a' + (value - 10);
            };

            std::string result;

            for (int i = 0; i < 8; i++) result += get_digit();
            result += '-';
            for (int i = 0; i < 4; i++) result += get_digit();
            result += "-4"; // version
            for (int i = 0; i < 3; i++) result += get_digit();
            result += '-';
            for (int i = 0; i < 4; i++) result += get_digit();
            result += '-';
            for (int i = 0; i < 12; i++) result += get_digit();

            return result;
        }
    }

    enum class Result {
        Ok = 0,
        OpenPipeFailed,
        PipeNotOpen,
        ReadPipeFailed,
        WritePipeFailed,
        HandshakeFailed,
        SetActivityFailed
    };

    struct IpcMessage {
        uint32_t op_code;
        std::string message;
    };

    class Pipe {
    public:
        virtual Result Open() = 0;
        virtual Result Close() = 0;
        virtual Result Read(IpcMessage* message) = 0;
        virtual Result Write(uint32_t op_code, std::string message) = 0;
    };

    #ifdef _WIN32
    class WindowsPipe final : public Pipe {
    public:
        ~WindowsPipe() {
            Close();
        }
        
        Result Open() override {
            if (pipe_handle) return Result::Ok;

            // Create pipe handle
            HANDLE pipe = CreateFileA(
                "\\\\.\\pipe\\discord-ipc-0",
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (pipe != INVALID_HANDLE_VALUE) {
                pipe_handle = pipe;

                DWORD mode = PIPE_READMODE_BYTE;
                if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL))
                    return Result::OpenPipeFailed;
            } else {
                return Result::OpenPipeFailed;
            }

            return Result::Ok;
        }

        Result Close() override {
            if (!pipe_handle) return Result::PipeNotOpen;
            CloseHandle(pipe_handle);
            return Result::Ok;
        }

        template<size_t N>
        Result ReadBytes(std::array<std::byte, N>* buffer) {
            DWORD bytes_read;
            if (!ReadFile(pipe_handle, buffer->data(), buffer->size(), &bytes_read, 0)
                || bytes_read != buffer->size()) {
                return Result::ReadPipeFailed;
            }
            return Result::Ok;
        }

        Result Read(IpcMessage* message) override {
            std::array<std::byte, 4> op_code_bytes, msg_len_bytes;

            Result result;
            if (result = ReadBytes(&op_code_bytes); result != Result::Ok) return result;
            if (result = ReadBytes(&msg_len_bytes); result != Result::Ok) return result;

            message->op_code = std::bit_cast<uint32_t>(op_code_bytes);
            uint32_t msg_len = std::bit_cast<uint32_t>(msg_len_bytes);
            
            std::vector<char> buffer(msg_len);
            DWORD bytes_read;
            if (!ReadFile(pipe_handle, buffer.data(), msg_len, &bytes_read, 0) || bytes_read != msg_len) {
                return Result::ReadPipeFailed;
            }
            message->message = std::string(buffer.begin(), buffer.end());

            return Result::Ok;
        }

        Result Write(uint32_t op_code, std::string message) override {
            uint32_t length = static_cast<uint32_t>(message.length());

            std::vector<char> buffer(8 + length);
            std::memcpy(buffer.data(), &op_code, 4);
            std::memcpy(buffer.data() + 4, &length, 4);
            std::memcpy(buffer.data() + 8, message.data(), length);

            DWORD bytes_written;
            return WriteFile(
                pipe_handle,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytes_written, 0
            ) && bytes_written == buffer.size() ? Result::Ok : Result::WritePipeFailed;
        }
    private:
        HANDLE pipe_handle;
    };

    #endif

    #pragma region Activity Types

    class Timestamps {
    public:
        void SetStart(int64_t seconds) { start = seconds; }
        void SetEnd(int64_t seconds) { end = seconds; }
        int64_t GetStart() const { return start; }
        int64_t GetEnd() const { return end; }

        std::string TsJson() const {
            std::string result = "{";

            if (start != 0)
                result += std::format("\"start\":{},", start);

            if (end != 0)
                result += std::format("\"end\":{}", end);

            if (result.ends_with(","))
                result.erase(result.length() - 1);
            return result + "}";
        }
    private:
        int64_t start = 0;
        int64_t end = 0;
    };

    class Party {
    public:
        void SetId(std::string id) {
            this->id = id;
        }
        std::string GetId() {
            return id;
        }

        /**
         * @param size Must be greater equals 0
         */
        void SetCurrentSize(int size) {
            assert(size >= 0);
            current_size = size;
        }
        int GetCurrentSize() const {
            return current_size;
        }

        /**
         * @param size Must be greater equals 0 and the current size
         */
        void SetMaxSize(int size) {
            assert(size >= 0 && size >= current_size);
            max_size = size;
        }
        int GetMaxSize() const {
            return max_size;
        }

        std::string ToJson() const {
            std::string result = "{";

            if (id.length() > 0)
                result += std::format("\"id\":\"{}\",", id);

            if (current_size != 0 || max_size != 0)
                result += std::format("\"size\":[{}, {}],", current_size, max_size);

            if (result.ends_with(","))
                result.erase(result.length() - 1);
            return result + "}";
        }
    private:
        std::string id;
        int current_size = 0;
        int max_size = 0;
    };

    class Assets {
    public:
        void SetLargeImage(std::string image) {
            large_image = image;
        }
        std::string GetLargeImage() const {
            return large_image;
        }

        void SetLargeImageText(std::string text) {
            large_text = text;
        }
        std::string GetLargeImageText() const {
            return large_text;
        }

        void SetSmallImage(std::string image) {
            small_image = image;
        }
        std::string GetSmallImage() const {
            return small_image;
        }

        void SetSmallImageText(std::string text) {
            small_text = text;
        }
        std::string GetSmallImageText() const {
            return small_text;
        }

        std::string ToJson() const {
            std::string result = "{";

            if (large_image.length() > 0)
                result += std::format("\"large_image\":\"{}\",", large_image);
            if (large_text.length() > 0)
                result += std::format("\"large_text\":\"{}\",", large_text);
            if (small_image.length() > 0)
                result += std::format("\"small_image\":\"{}\",", small_image);
            if (small_text.length() > 0)
                result += std::format("\"small_text\":\"{}\",", small_text);

            if (result.ends_with(","))
                result.erase(result.length() - 1);
            return result + "}";
        }
    private:
        std::string large_image;
        std::string large_text;
        std::string small_image;
        std::string small_text;
    };

    enum class ActivityType {
        Playing = 0,
        Listening = 2,
        Watching = 3,
        Competing = 5
    };

    class Activity {
    public:
        /**
         * @brief Set the client/application id
         * @param client_id 0 = use id from client
         */
        void SetClientId(uint64_t client_id) {
            this->client_id = client_id;
        }
        uint64_t GetClientId() const {
            return this->client_id;
        }

        /**
         * @brief Sets the name of the activity
         * @param name Length must be greater than 0
         */
        void SetName(std::string name) {
            assert(name.length() > 0);
            this->name = name;
        }
        std::string GetName() const {
            return name;
        }

        void SetType(ActivityType type) {
            this->type = type;
        }
        ActivityType GetType() const {
            return type;
        }

        void SetDetails(std::string details) {
            this->details = details;
        }
        std::string GetDetails() const {
            return details;
        }

        void SetState(std::string state) {
            this->state = state;
        }
        std::string GetState() const {
            return state;
        }

        Timestamps& GetTimestamps() {
            return timestamps;
        }

        void SetParty(std::optional<Party> party) {
            this->party = party;
        }
        /**
         * Note: Party is std::nullopt by default
         */
        Party* GetParty() {
            return party.has_value() ? &party.value() : nullptr;
        }

        Assets& GetAssets() {
            return assets;
        }

        std::string ToJson() const {
            std::string result = "{";

            if (name.length() > 0)
                result += std::format("\"name\":\"{}\",", name);

            if (client_id != 0)
                result += std::format("\"client_id\":\"{}\",", client_id);

            // activity type
            result += std::format("\"type\":{},", static_cast<int>(type));

            // details
            if (details.length() > 0)
                result += std::format("\"details\":\"{}\",", details);

            // state
            if (state.length() > 0)
                result += std::format("\"state\":\"{}\",", state);

            // timestamps
            result += std::format("\"timestamps\":{},", timestamps.TsJson());

            // party
            if (party.has_value())
                result += std::format("\"party\":{},", party.value().ToJson());

            // assets
            result += std::format("\"assets\":{},", assets.ToJson());

            // remove trailing comma
            if (result.ends_with(","))
                result.erase(result.length() - 1);
            return result + "}";
        }
    private:
        uint64_t client_id = 0;
        std::string name;
        ActivityType type = ActivityType::Playing;
        std::string details;
        std::string state;
        Timestamps timestamps;
        std::optional<Party> party = std::nullopt;
        Assets assets;
    };

    #pragma endregion

    class Client {
    public:
        Client(uint64_t client_id) : client_id(client_id) {
            #if _WIN32
            pipe = std::make_shared<WindowsPipe>();
            #else
            #pragma GCC error "Non-Windows operating systems are not yet supported"
            #endif
        }

        Result Connect() {
            Result result;

            // Open pipe if needed
            if (result = pipe->Open(); result != Result::Ok) return result;
            if (result = pipe->Write(0, std::format("{{\"v\":1,\"client_id\":\"{}\"}}", client_id)); result != Result::Ok) return result;
            
            // Wait for dispatch event
            IpcMessage message;
            if (result = pipe->Read(&message); result != Result::Ok) return result;
            return message.op_code == 1 ? Result::Ok : Result::HandshakeFailed;
        }

        Result UpdateActivity(const Activity& activity) {
            #if _WIN32
            int pid = GetCurrentProcessId();
            #else // unix
            #include <unistd.h>
            int pid = getpid();
            #endif

            std::string msg = std::format(
                "{{\"cmd\":\"SET_ACTIVITY\",\"args\":{{\"pid\":{},\"activity\":{}}},\"nonce\":\"{}\"}}",
                pid,
                activity.ToJson(),
                UUID::GenerateUUIDv4()
            );

            Result result;
            if (result = pipe->Write(1, msg); result != Result::Ok) return result;

            // Wait for response
            IpcMessage message;
            if (result = pipe->Read(&message); result != Result::Ok) return result;
            return message.op_code == 1
                && message.message.find("\"evt\":\"ERROR\"") == std::string::npos
                ? Result::Ok : Result::SetActivityFailed;
        }

        Result ClearActivity() {
            #if _WIN32
            int pid = GetCurrentProcessId();
            #else // unix
            #include <unistd.h>
            int pid = getpid();
            #endif

            std::string msg = std::format(
                "{{\"cmd\":\"SET_ACTIVITY\",\"args\":{{\"pid\":{},\"activity\":{{}}}},\"nonce\":\"{}\"}}",
                pid,
                UUID::GenerateUUIDv4()
            );

            Result result;
            if (result = pipe->Write(1, msg); result != Result::Ok) return result;

            // Wait for response
            IpcMessage message;
            if (result = pipe->Read(&message); result != Result::Ok) return result;
            return message.op_code == 1
                && message.message.find("\"evt\":\"ERROR\"") == std::string::npos
                ? Result::Ok : Result::SetActivityFailed;
        }
    private:
        std::shared_ptr<Pipe> pipe;
        uint64_t client_id;

    };
}

#endif