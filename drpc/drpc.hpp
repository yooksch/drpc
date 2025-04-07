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
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <regex>
#include <sstream>
#include <thread>
#include <variant>
#include <vector>
#include <string>
#include <array>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace DiscordRichPresence {
    namespace JSON {
        class JsonWriter;

        class JsonSerializable {
        public:
            virtual ~JsonSerializable() = default;
            virtual void ToJson(JsonWriter* writer) const = 0;
        };

        class JsonValue {
        public:
            JsonValue(const std::string& string) : value(string) {}
            JsonValue(const char* string) : value(string) {}
            JsonValue(int32_t n) : value(n) {}
            JsonValue(uint32_t n) : value(n) {}
            JsonValue(int64_t n) : value(n) {}
            JsonValue(uint64_t n) : value(n) {}
            JsonValue(float n) : value(n) {}
            JsonValue(double n) : value(n) {}
            JsonValue(bool b) : value(b) {}
            JsonValue(std::shared_ptr<JsonSerializable> serializable) : value(serializable) {}
            JsonValue(std::vector<JsonValue> vec) : value(vec) {}
            JsonValue(std::map<std::string, JsonValue> map) : value(map) {}

            template<typename T>
            T get() const {
                return std::get<T>(value);
            }

            void ToJson(JsonWriter* writer) const;
        private:
            std::variant<
                std::string,
                int32_t,
                uint32_t,
                int64_t,
                uint64_t,
                float,
                double,
                bool,
                std::vector<JsonValue>,
                std::map<std::string, JsonValue>,
                std::shared_ptr<JsonSerializable>
                > value;
        };

        template<typename T>
        concept JsonValueType = requires(T value) {
            JsonValue(value);
        };

        class JsonWriter {
        public:
        void BeginObject() {
                WriteRaw("{");
                current_object_sizes.emplace_back(0);
            }

            void EndObject() {
                WriteRaw("}");
                current_object_sizes.pop_back();
            }

            std::string ToString() const {
                return s.str();
            }

            void Write(const JsonSerializable& object) {
                object.ToJson(this);
            }

            void Write(const JsonValue value) {
                value.ToJson(this);
            }
            
            void WriteRaw(const char* str) {
                s.write(str, strlen(str));
            }

            void PendMember(const char* key) {
                assert(current_object_sizes.size() > 0);

                if (current_object_sizes.back() > 0)
                    WriteRaw(",");

                Write(key);
                WriteRaw(":");
                current_object_sizes.back()++;
            }

            template<JsonValueType T>
            void Put(const std::string& key, T value) {
                assert(current_object_sizes.size() > 0);

                if (current_object_sizes.back() > 0)
                    WriteRaw(",");

                Write(key);
                WriteRaw(":");
                Write(JsonValue(value));
                current_object_sizes.back()++;
            }
        private:
            std::stringstream s;
            std::vector<size_t> current_object_sizes;
        };

        inline void JsonValue::ToJson(JsonWriter* writer) const {
            if (std::holds_alternative<std::string>(value)) {
                writer->WriteRaw("\"");
                writer->WriteRaw(std::get<std::string>(value).c_str());
                writer->WriteRaw("\"");
            } else if (std::holds_alternative<int32_t>(value)) {
                writer->WriteRaw(std::to_string(std::get<int32_t>(value)).c_str());
            } else if (std::holds_alternative<uint32_t>(value)) {
                writer->WriteRaw(std::to_string(std::get<uint32_t>(value)).c_str());
            } else if (std::holds_alternative<int64_t>(value)) {
                writer->WriteRaw(std::to_string(std::get<int64_t>(value)).c_str());
            } else if (std::holds_alternative<uint64_t>(value)) {
                writer->WriteRaw(std::to_string(std::get<uint64_t>(value)).c_str());
            } else if (std::holds_alternative<float>(value)) {
                writer->WriteRaw(std::to_string(std::get<float>(value)).c_str());
            } else if (std::holds_alternative<double>(value)) {
                writer->WriteRaw(std::to_string(std::get<double>(value)).c_str());
            } else if (std::holds_alternative<bool>(value)) {
                writer->WriteRaw(std::get<bool>(value) ? "true" : "false");
            } else if (std::holds_alternative<std::shared_ptr<JsonSerializable>>(value)){
                auto s = std::get<std::shared_ptr<JsonSerializable>>(value);
                s->ToJson(writer);
            } else if (std::holds_alternative<std::vector<JsonValue>>(value)) {
                writer->WriteRaw("[");

                auto items = std::get<std::vector<JsonValue>>(value);
                for (size_t i = 0; i < items.size(); i++) {
                    items[i].ToJson(writer);
                    if (i + 1 < items.size())
                        writer->WriteRaw(",");
                }

                writer->WriteRaw("]");
            } else if (std::holds_alternative<std::map<std::string, JsonValue>>(value)) {
                writer->WriteRaw("{");

                for (auto const& [key, value] : std::get<std::map<std::string, JsonValue>>(value))
                    writer->Put(key, value);

                writer->WriteRaw("}");
            }
        }
    }

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
        PipeNotOpen,
        OpenPipeFailed,
        ReadPipeFailed,
        WritePipeFailed,
        HandshakeFailed,
        SetActivityFailed,
        UnknownError,
        ReadPipeNoData
    };

    enum class LogLevel {
        Info,
        Warn,
        Error,
        Trace
    };

    enum class Event {
        Connected,
        Disconnected
    };

    inline const char* ResultToString(Result result) {
        switch (result) {
        case DiscordRichPresence::Result::Ok:
            return "Ok";
        case DiscordRichPresence::Result::PipeNotOpen:
            return "PipeNotOpen";
        case DiscordRichPresence::Result::OpenPipeFailed:
            return "OpenPipeFailed";
        case DiscordRichPresence::Result::ReadPipeFailed:
            return "ReadPipeFailed";
        case DiscordRichPresence::Result::WritePipeFailed:
            return "WritePipeFailed";
        case DiscordRichPresence::Result::HandshakeFailed:
            return "HandshakeFailed";
        case DiscordRichPresence::Result::SetActivityFailed:
            return "SetActivity";
        case DiscordRichPresence::Result::UnknownError:
            return "UnknownError";
        case DiscordRichPresence::Result::ReadPipeNoData:
            return "ReadPipeNoData";
        }
    }

    inline const char* ResultToDescription(Result result) {
        switch (result) {
        case DiscordRichPresence::Result::Ok:
            return "Ok";
        case DiscordRichPresence::Result::PipeNotOpen:
            return "Named pipe is not open";
        case DiscordRichPresence::Result::OpenPipeFailed:
            return "Failed to open named pipe";
        case DiscordRichPresence::Result::ReadPipeFailed:
            return "Failed to read from named pipe";
        case DiscordRichPresence::Result::WritePipeFailed:
            return "Failed to write to named pipe";
        case DiscordRichPresence::Result::HandshakeFailed:
            return "Failed to handshake with Discord client";
        case DiscordRichPresence::Result::SetActivityFailed:
            return "Failed to set Discord activity";
        case DiscordRichPresence::Result::UnknownError:
            return "An unknown occured";
        case DiscordRichPresence::Result::ReadPipeNoData:
            return "Reading from named pipe returned no data";
        }
    }

    struct IpcMessage {
        uint32_t op_code;
        std::string message;
        std::string nonce;
    };

    template<>
    struct std::formatter<IpcMessage> {
        constexpr auto parse(std::format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(const IpcMessage& message, std::format_context& ctx) const {
            return std::format_to(ctx.out(), "Nonce:{} Op:{} Msg:{}", message.nonce.length() > 0 ? message.nonce : "NONE", message.op_code, message.message);
        }
    };

    class Pipe {
    public:
        virtual Result Open() = 0;
        virtual Result Close() = 0;
        virtual Result Read(IpcMessage* message, bool peek = false) = 0;
        virtual void CancelIo() = 0;
        virtual Result Write(uint32_t op_code, std::string message) = 0;
        virtual bool IsOpen() = 0;
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
                NULL,
                NULL,
                OPEN_EXISTING,
                NULL,
                NULL
            );

            if (pipe != INVALID_HANDLE_VALUE) {
                pipe_handle = pipe;

                DWORD mode = PIPE_READMODE_BYTE;
                if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
                    Close();
                    return Result::OpenPipeFailed;
                }
            } else {
                return Result::OpenPipeFailed;
            }

            return Result::Ok;
        }

        Result Close() override {
            CloseHandle(pipe_handle);
            pipe_handle = NULL;
            return Result::Ok;
        }

        template<size_t N>
        Result ReadBytes(std::array<std::byte, N>* buffer, bool peek = false) {
            DWORD bytes_read;
            if (peek) {
                DWORD bytes_available;
                if (PeekNamedPipe(pipe_handle, NULL, NULL, NULL, &bytes_available, NULL)) {
                    if (bytes_available > 0)
                        goto read;
                    else
                        return Result::ReadPipeNoData;
                }
                return Result::ReadPipeFailed;
            } else {
                read:
                if (!ReadFile(pipe_handle, buffer->data(), buffer->size(), &bytes_read, 0)
                    || bytes_read != buffer->size()) {
                    return Result::ReadPipeFailed;
                }
            }
            return Result::Ok;
        }

        Result Read(IpcMessage* message, bool peek) override {
            static std::regex nonce_re(R"(\"nonce\":\"([a-z-A-Z0-9\-]+)\")");

            std::array<std::byte, 4> op_code_bytes, msg_len_bytes;

            Result result;
            if (result = ReadBytes(&op_code_bytes, peek); result != Result::Ok) return result;
            if (result = ReadBytes(&msg_len_bytes, peek); result != Result::Ok) return result;

            message->op_code = std::bit_cast<uint32_t>(op_code_bytes);
            uint32_t msg_len = std::bit_cast<uint32_t>(msg_len_bytes);
            
            std::vector<char> buffer(msg_len);
            DWORD bytes_read;

            if (peek) {
                DWORD bytes_available;
                if (PeekNamedPipe(pipe_handle, NULL, NULL, NULL, &bytes_available, NULL)) {
                    if (bytes_available > 0)
                        goto read;
                    else
                        return Result::ReadPipeNoData;
                } else {
                    return Result::ReadPipeFailed;
                }
            } else {
                read:
                if (!ReadFile(pipe_handle, buffer.data(), msg_len, &bytes_read, 0) || bytes_read != msg_len) {
                    return Result::ReadPipeFailed;
                }
            }
            message->message = std::string(buffer.begin(), buffer.end());
            
            // Get nonce
            std::smatch match;
            if (std::regex_search(message->message, match, nonce_re))
                message->nonce = match.str(1);

            return Result::Ok;
        }

        void CancelIo() override {
            ::CancelIo(pipe_handle);
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

        bool IsOpen() override {
            return pipe_handle && GetNamedPipeHandleState(pipe_handle, NULL, NULL, NULL, NULL, NULL, 0);
        }
    private:
        HANDLE pipe_handle;
    };

    #endif

    #pragma region Activity Types

    class Timestamps : public JSON::JsonSerializable {
    public:
        void SetStart(int64_t seconds) { start = seconds; }
        void SetEnd(int64_t seconds) { end = seconds; }
        int64_t GetStart() const { return start; }
        int64_t GetEnd() const { return end; }

        void ToJson(JSON::JsonWriter* writer) const override {
            writer->BeginObject();
            if (start > 0) writer->Put("start", start);
            if (end > 0) writer->Put("end", end);
            writer->EndObject();
        }
    private:
        int64_t start = 0;
        int64_t end = 0;
    };

    class Party : public JSON::JsonSerializable {
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

        void ToJson(JSON::JsonWriter* writer) const override {
            writer->BeginObject();

            if (id.length() > 0)
                writer->Put("id", id);

            if (current_size != 0 || max_size != 0)
                writer->Put("size", std::vector<JSON::JsonValue> {current_size, max_size});

            writer->EndObject();
        }
    private:
        std::string id;
        int current_size = 0;
        int max_size = 0;
    };

    class Assets : public JSON::JsonSerializable {
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

        void ToJson(JSON::JsonWriter* writer) const override {
            writer->BeginObject();

            if (large_image.length() > 0)
                writer->Put("large_image", large_image);

            if (large_text.length() > 0)
                writer->Put("large_text", large_text);

            if (small_image.length() > 0)
                writer->Put("small_image", small_image);

            if (small_text.length() > 0)
                writer->Put("small_text", small_text);
            
            writer->EndObject();
        }
    private:
        std::string large_image;
        std::string large_text;
        std::string small_image;
        std::string small_text;
    };

    class Button : public JSON::JsonSerializable {
    public:
        Button(std::string label, std::string url) {
            SetLabel(label);
            SetUrl(url);
        }

        void SetLabel(std::string label) {
            assert(label.length() < 32);
            this->label = label;
        }
        std::string GetLabel() const {
            return label;
        }

        void SetUrl(std::string url) {
            assert(url.length() < 512);
            this->url = url;
        }
        std::string GetUrl() const {
            return url;
        }

        void ToJson(JSON::JsonWriter* writer) const override {
            writer->BeginObject();
            writer->Put("label", label);
            writer->Put("url", url);
            writer->EndObject();
        }
    private:
        std::string label;
        std::string url;
    };

    enum class ActivityType {
        Playing = 0,
        Listening = 2,
        Watching = 3,
        Competing = 5
    };

    class Activity : public JSON::JsonSerializable {
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

        std::shared_ptr<Timestamps> GetTimestamps() {
            return timestamps;
        }

        void SetParty(std::shared_ptr<Party> party) {
            this->party = party;
        }
        /**
         * Note: Party is std::nullopt by default
         */
        std::shared_ptr<Party> GetParty() {
            return party != nullptr ? party : nullptr;
        }

        std::shared_ptr<Assets> GetAssets() {
            return assets;
        }

        void AddButton(std::shared_ptr<Button> button) {
            assert(buttons.size() < 2);
            buttons.emplace_back(button);
        }

        void ClearButtons() {
            buttons.clear();
        }

        void ToJson(JSON::JsonWriter* writer) const override {
            writer->BeginObject();

            if (name.length() > 0)
                writer->Put("name", name);

            if (client_id != 0)
                writer->Put("client_id", client_id);

            writer->Put("type", static_cast<int>(type));

            if (details.length() > 0)
                writer->Put("details", details);

            if (state.length() > 0)
                writer->Put("state", state);

            writer->Put("timestamps", timestamps);
            
            if (party != nullptr)
                writer->Put("party", party);

            writer->Put("assets", assets);
            
            std::vector<JSON::JsonValue> j_buttons;
            for (const auto& btn : buttons)
                j_buttons.emplace_back(btn);

            if (j_buttons.size() > 0)
                writer->Put("buttons", j_buttons);

            writer->EndObject();
        }
    private:
        uint64_t client_id = 0;
        std::string name;
        ActivityType type = ActivityType::Playing;
        std::string details;
        std::string state;
        std::shared_ptr<Timestamps> timestamps = std::make_shared<Timestamps>();
        std::shared_ptr<Party> party = nullptr;
        std::shared_ptr<Assets> assets = std::make_shared<Assets>();
        std::vector<std::shared_ptr<Button>> buttons;
    };

    #pragma endregion

    struct ClientSettings {
        bool auto_reconnect = true;
        uint64_t reconnect_timeout_ms = 5000;
    };

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

            JSON::JsonWriter writer;
            writer.BeginObject();
            writer.Put("v", 1);
            writer.Put("client_id", std::to_string(client_id));
            writer.EndObject();

            if (result = pipe->Write(0, writer.ToString()); result != Result::Ok) return result;
            
            // Wait for dispatch event
            IpcMessage message;

            // Dispatch events do not provide a nonce; therefore, we ignore the error
            if (result = pipe->Read(&message); result != Result::Ok) return result;
            bool success = message.op_code == 1;

            if (success) {
                event_callback(Event::Connected);
            }

            log_callback(
                Result::Ok,
                LogLevel::Trace,
                std::format("{}", message),
                message
            );

            return success ? Result::Ok : Result::HandshakeFailed;
        }

        Result Disconnect() {
            return pipe->Close();
        }

        Result Reconnect() {
            if (Result result = pipe->Close(); result != Result::Ok) return result;
            return Connect();
        }

        void UpdateActivity(const std::shared_ptr<Activity> activity, std::function<void(Result result, IpcMessage ipc_message)> callback) {
            #if _WIN32
            int pid = GetCurrentProcessId();
            #else // unix
            #include <unistd.h>
            int pid = getpid();
            #endif

            auto nonce = UUID::GenerateUUIDv4();

            JSON::JsonWriter writer;
            writer.BeginObject();
            writer.Put("cmd", "SET_ACTIVITY");

            writer.PendMember("args");
            writer.BeginObject();
            writer.Put("pid", static_cast<long long>(pid));
            writer.Put("activity", activity);
            writer.EndObject();

            writer.Put("nonce", nonce);
            writer.EndObject();

            outgoing_messages.emplace(IpcMessage {
                .op_code = 1,
                .message = writer.ToString(),
                .nonce = nonce
            });

            callbacks[nonce] = callback;
            last_activity = activity;
        }

        void ClearActivity(std::function<void(Result result, IpcMessage ipc_message)> callback) {
            #if _WIN32
            int pid = GetCurrentProcessId();
            #else // unix
            #include <unistd.h>
            int pid = getpid();
            #endif

            auto nonce = UUID::GenerateUUIDv4();
            JSON::JsonWriter writer;
            writer.BeginObject();
            writer.Put("cmd", "SET_ACTIVITY");
            writer.PendMember("args");
            writer.BeginObject();
            writer.Put("pid", pid);
            writer.PendMember("activity");

            // Make an empty object
            writer.BeginObject();
            writer.EndObject();

            // End args object
            writer.EndObject();

            writer.Put("nonce", nonce);
            writer.EndObject();

            outgoing_messages.emplace(IpcMessage {
                .op_code = 1,
                .message = writer.ToString(),
                .nonce = nonce
            });

            callbacks[nonce] = callback;
            last_activity = nullptr;
        }

        Result Run() {
            Result result;

            while (true) {
                if (!pipe->IsOpen()) {
                    if (settings.auto_reconnect) {
                        log_callback(Result::PipeNotOpen, LogLevel::Error, "Pipe handle is invalid. Attempting to reconnect", std::nullopt);

                        if (result = Connect(); result == Result::Ok) {
                            log_callback(Result::Ok, LogLevel::Info, "Reconnected", std::nullopt);

                            if (last_activity != nullptr) {
                                UpdateActivity(last_activity, [this](auto result, auto message) {
                                    if (result == Result::Ok) {
                                        log_callback(result, LogLevel::Info, "Re-used last activity", message);
                                    } else {
                                        log_callback(result, LogLevel::Error, "Failed to use last activity", message);
                                    }
                                });
                            }
                        } else {
                            log_callback(result, LogLevel::Error, "Failed to reconnect", std::nullopt);
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(settings.reconnect_timeout_ms));
                        continue;
                    }

                    // Wait 100ms before retrying
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                // Send queued messages
                while (outgoing_messages.size() > 0) {
                    auto msg = outgoing_messages.front();
                    outgoing_messages.pop();
                    if (result = pipe->Write(msg.op_code, msg.message); result != Result::Ok) {
                        log_callback(result, LogLevel::Error, ResultToDescription(result), msg);
                        
                        if (callbacks.contains(msg.nonce)) {
                            callbacks[msg.nonce](result, msg);
                            callbacks.erase(msg.nonce);
                        }

                        continue;
                    }
                }

                IpcMessage msg;
                if (result = pipe->Read(&msg, true); result != Result::Ok) {
                    // Timeouts are expected and necessary for the loop to continue
                    if (result == Result::ReadPipeNoData && pipe->IsOpen()) continue;

                    log_callback(result, LogLevel::Error, ResultToDescription(result), msg);
                    if (result == Result::ReadPipeFailed) {
                        pipe->Close(); // Close pipe handle
                        event_callback(Event::Disconnected);
                    }
                } else {
                    auto success = msg.message.find("\"evt\":\"ERROR\"") == std::string::npos && msg.op_code != 2;
                    log_callback(
                        success ? Result::Ok : Result::UnknownError,
                        LogLevel::Trace,
                        std::format("{}", msg),
                        msg
                    );
                }

                if (callbacks.contains(msg.nonce)) {
                    callbacks[msg.nonce](result, msg);
                    callbacks.erase(msg.nonce);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            return Result::Ok;
        }

        void SetLogCallback(std::function<void(Result result, LogLevel level, std::string message, std::optional<IpcMessage> ipc_message)> callback) {
            log_callback = callback;
        }

        void SetEventCallback(std::function<void(Event event)> callback) {
            event_callback = callback;
        }

        ClientSettings& GetSettings() {
            return settings;
        }
    private:
        ClientSettings settings;
        std::shared_ptr<Pipe> pipe;
        uint64_t client_id;
        std::queue<IpcMessage> outgoing_messages;
        std::map<std::string, std::function<void(Result result, IpcMessage ipc_message)>> callbacks;
        std::function<void(Result result, LogLevel level, std::string message, std::optional<IpcMessage> ipc_message)> log_callback = [](auto, auto, auto, auto){};
        std::function<void(Event event)> event_callback = [](auto){};
        std::shared_ptr<Activity> last_activity;
    };

    inline const char* LogLevelToString(LogLevel level) {
        switch (level) {
        case DiscordRichPresence::LogLevel::Info:
            return "INFO";
        case DiscordRichPresence::LogLevel::Warn:
            return "WARN";
        case DiscordRichPresence::LogLevel::Error:
            return "ERROR";
        case DiscordRichPresence::LogLevel::Trace:
            return "TRACE";
        }
    }
}

#endif