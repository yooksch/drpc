#include "drpc/drpc.hpp"

#include <chrono>
#include <cstdint>
#include <print>
#include <memory>

constexpr uint64_t APPLICATION_ID = 1355907951155740785;

int main() {
    DiscordRichPresence::Client client(APPLICATION_ID);

    client.SetEventCallback([](auto event) {
        if (event == DiscordRichPresence::Event::Connected)
            std::println("Connected");
        else if (event == DiscordRichPresence::Event::Disconnected)
            std::println("Disconnected");
    });

    client.SetLogCallback([](auto result, auto level, auto message, auto ipc_message) {
        std::println("[{}] [{}] {}", DiscordRichPresence::LogLevelToString(level), DiscordRichPresence::ResultToString(result), message);
    });

    auto result = client.Connect();
    std::println("Connect returned: {}", DiscordRichPresence::ResultToString(result));

    auto activity = std::make_shared<DiscordRichPresence::Activity>();
    activity->SetClientId(APPLICATION_ID);
    activity->SetName("drpc");
    activity->SetDetails("Line 1");
    activity->GetTimestamps()->SetStart(time(NULL));

    auto assets = activity->GetAssets();
    assets->SetLargeImage("my_image");
    assets->SetLargeImageText("You hovered over the large image");
    assets->SetSmallImage("my_image");
    assets->SetSmallImageText("I didn't have another image");

    auto party = std::make_shared<DiscordRichPresence::Party>();
    party->SetId("test");
    party->SetCurrentSize(2);
    party->SetMaxSize(5);
    activity->SetParty(party);
    activity->SetState("Party"); // State moves to the party field if party is not null

    // Buttons
    activity->AddButton(std::make_shared<DiscordRichPresence::Button>("Test", "https://yooksch.com"));
    activity->AddButton(std::make_shared<DiscordRichPresence::Button>("Test 2", "https://youtu.be/dQw4w9WgXcQ"));

    client.UpdateActivity(activity, [](DiscordRichPresence::Result result, auto) {
        std::println("Updated activity: {}", DiscordRichPresence::ResultToDescription(result));
    });

    std::thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        client.UpdateActivity(activity, [](auto, auto) {});
    }).detach();

    result = client.Run(); 
    std::println("Client exited: {} - {}", DiscordRichPresence::ResultToString(result), DiscordRichPresence::ResultToDescription(result));

    return 0;
}