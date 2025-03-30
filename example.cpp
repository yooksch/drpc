#include "drpc/drpc.hpp"

#include <cstdint>
#include <iostream>

constexpr uint64_t APPLICATION_ID = 1355907951155740785;

int main() {
    DiscordRichPresence::Client client(APPLICATION_ID);
    client.Connect();

    DiscordRichPresence::Activity activity;
    activity.SetClientId(APPLICATION_ID);
    activity.SetName("drpc");
    activity.SetDetails("Line 1");
    activity.GetTimestamps().SetStart(time(NULL));

    DiscordRichPresence::Assets* assets = &activity.GetAssets();
    assets->SetLargeImage("my_image");
    assets->SetLargeImageText("You hovered over the large image");
    assets->SetSmallImage("my_image");
    assets->SetSmallImageText("I didn't have another image");

    DiscordRichPresence::Party party;
    party.SetId("test");
    party.SetCurrentSize(2);
    party.SetMaxSize(5);
    activity.SetParty(party);
    activity.SetState("Party"); // State moves to the party field if party is not null

    auto result = client.UpdateActivity(activity);
    if (result == DiscordRichPresence::Result::Ok) {
        std::cout << "Successfully set activity!" << std::endl;
    } else {
        std::cout << "Failed to set activity (" << (int)result << ")" << std::endl;
    }

    std::cin.get(); // Keep process alive

    return 0;
}