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
#include "World.h"

// Add player scripts
class PlayerbotsEventsPlayer : public PlayerScript
{
public:
    PlayerbotsEventsPlayer() : PlayerScript("PlayerbotsEventsPlayer") { }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        sPlayerbotsEvents->OnUpdateZone(player, newZone, newArea);
    }
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
    }
};


// Add all scripts in one
void AddPlayerbotsEventsScripts()
{
    new PlayerbotsEventsPlayer();
    new PlayerbotsEventsWorld();
    new PlayerbotsEventsPlayerbots();
}
