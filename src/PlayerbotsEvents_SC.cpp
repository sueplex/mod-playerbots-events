/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "PlayerbotsEvents.h"

#include "ScriptMgr.h"
#include "Player.h"
#include "PvpBotMgr.h"
#include "PvpPlayerbotMgr.h"
#include "World.h"

// Add player scripts
class PlayerbotsEventsPlayer : public PlayerScript
{
public:
    PlayerbotsEventsPlayer() : PlayerScript("PlayerbotsEventsPlayer") { }

    /*void OnLogin(Player* player) override
    {
        if (!player->GetSession()->IsBot())
        {
            std::cout << "Session IsBot false\n";
            sPvpPlayerbotsMgr->AddPvpPlayerbotData(player, false);
            sPvpMgr->OnPlayerLogin(player);
            std::cout << "OnLogin Handled\n";
        }
    }*/

    void OnAfterUpdate(Player* player, uint32 diff) override
    {
        /*if (PlayerbotAI* botAI = GET_PVPPLAYERBOT_AI(player))
        {
            botAI->UpdateAI(diff);
        }*/

        if (PvpPlayerbotMgr* playerbotMgr = GET_PVPPLAYERBOT_MGR(player))
        {
            playerbotMgr->UpdateAI(diff);
        }
    }

    /*bool OnBeforeAchiComplete(Player* player, AchievementEntry const* achievement) override
    {
        if (sPvpMgr->IsPvpBot(player))
        {
            return false;
        }
        return true;
    }*/
};


// Add World scripts
class PlayerbotsEventsWorld : public WorldScript
{
public:
    PlayerbotsEventsWorld() : WorldScript("PlayerbotsEventsWorld") {}

    void OnBeforeWorldInitialized() override
    {
        sPlayerbotsEvents->Initialize();
    }
};

// Add Playerbot Scripts
class PlayerbotsEventsPlayerbots: public PlayerbotScript
{
public:
    PlayerbotsEventsPlayerbots():  PlayerbotScript("PlayerbotsEventsPlayerbots") {}

    void OnPlayerbotUpdate(uint32 diff) override
    {
        sPvpMgr->UpdateAI(diff);
        sPvpMgr->UpdateSessions();
    }

    void OnPlayerbotUpdateSessions(Player* player) override
    {
        if (player)
            if (PvpPlayerbotMgr* playerbotMgr = GET_PVPPLAYERBOT_MGR(player)) {
                std::cout << "updating\n";
                playerbotMgr->UpdateSessions();
                std::cout << "updated\n";
            }
    }

    void OnPlayerbotPacketSent(Player* player, WorldPacket const* packet) override
    {
        if (!player)
            return;

        if (PlayerbotAI* botAI = GET_PVPPLAYERBOT_AI(player))
        {
            botAI->HandleBotOutgoingPacket(*packet);
        }
        if (PvpPlayerbotMgr* playerbotMgr = GET_PVPPLAYERBOT_MGR(player))
        {
            playerbotMgr->HandleMasterOutgoingPacket(*packet);
        }
    }
};

class PlayerbotsEventsServerScript : public ServerScript
{
public:
    PlayerbotsEventsServerScript() : ServerScript("PlayerbotsEventsServerScript") {}

    void OnPacketReceived(WorldSession* session, WorldPacket const& packet) override
    {
        if (Player* player = session->GetPlayer())
            if (PvpPlayerbotMgr* playerbotMgr = GET_PVPPLAYERBOT_MGR(player))
                playerbotMgr->HandleMasterIncomingPacket(packet);
    }
};



// Add all scripts in one
void AddPlayerbotsEventsScripts()
{
    new PlayerbotsEventsPlayer();
    new PlayerbotsEventsWorld();
    new PlayerbotsEventsPlayerbots();
    new PlayerbotsEventsServerScript();
}
