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
#include "Config.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "Player.h"


PlayerbotsEvents* PlayerbotsEvents::instance()
{
    static PlayerbotsEvents instance;
    return &instance;
}

bool PlayerbotsEvents::Initialize()
{
    enabled = sConfigMgr->GetOption<bool>("PlayerbotsEvents.Enable", true);
    if (!enabled)
    {
        return false;
    }
    return true;
}


void PlayerbotsEvents::OnUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/)
{
    if (!enabled || player->GetSession()->GetRemoteAddress() == "bot")
        return;

    AreaTableEntry const *area = sAreaTableStore.LookupEntry(newZone);

    if ((area->flags & 312) == 312) {
        std::cout << area->area_name << " is city\n";
    }
    std::cout << "Add ~level " << player << "to " << area->area_name << "\n";
}
