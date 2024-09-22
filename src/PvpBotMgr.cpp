

#include "PvpBotMgr.h"
#include "AiFactory.h"
#include "AccountMgr.h"
#include "CellImpl.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "Define.h"
#include "FleeManager.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "MapMgr.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotFactory.h"
#include "PvpPlayerbotMgr.h"
#include "Random.h"
#include "RandomPlayerbotFactory.h"
#include "ServerFacade.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "World.h"

#include <cstdlib>
#include <iostream>
#include <random>
#include <stdint.h>
#include <vector>


WorldLocation SW_Alli_1(0, -8816.233f, 650.658f, 94.549f, 1.36f);
WorldLocation SW_Alli_2(0, -8813.371f, 650.422f, 94.542f, 2.14f);
WorldLocation SW_Alli_3(0, -8834.425f, 636.413f, 94.474f, 5.73f);
WorldLocation SW_Alli_4(0, -8814.245f, 616.836f, 94.839f, 2.21f);
WorldLocation SW_Alli_5(0, -8853.779f, 617.489f, 95.987f, 0.13f);
WorldLocation SW_Alli_6(0, -8843.036f, 650.025f, 96.693f, 3.43f);
WorldLocation SW_Alli_7(0, -8856.855f, 649.924f, 97.609f, 5.31f);
WorldLocation SW_Alli_8(0, -8864.268f, 637.827f, 95.945f, 0.09f);

WorldLocation SW_Horde_1(0, -9104.613f, 290.815f, 93.601f, 1.871f);


//WorldLocation ORG_Alli_1(1, 1285.882f, -4340.367f, 33.098f, 0);

PvpBotMgr::PvpBotMgr() : PvpPlayerbotHolder() {
    WorldLocation loc(1, 1285.882f, -4340.367f, 33.098f, 0);

    // Stormwind Alliance
    locsPerRaidCache[1].push_back(SW_Alli_1);
    locsPerRaidCache[1].push_back(SW_Alli_2);
    locsPerRaidCache[1].push_back(SW_Alli_3);
    locsPerRaidCache[1].push_back(SW_Alli_4);
    locsPerRaidCache[1].push_back(SW_Alli_5);
    locsPerRaidCache[1].push_back(SW_Alli_6);
    locsPerRaidCache[1].push_back(SW_Alli_7);
    locsPerRaidCache[1].push_back(SW_Alli_8);

    // Stormwind Horde
    locsPerRaidCache[3].push_back(SW_Horde_1);
}

PvpBotMgr::~PvpBotMgr() {

}

void PvpBotMgr::UpdateAIInternal(uint32 elapsed, bool minimal)
{
    // Get and Set Value for bot_count here?
    uint32 maxAllowedBotCount = 1;

    uint32 fraidStart = GetEventValue(0, "fraid");
    if (!fraidStart) {
        std::cout << "not valid: " << fraidStart << "\n";
        std::cout << "resetting validity\n";
        SetEventValue(0, "fraid", 1, 300);
    }

    GetBots();
    std::list<uint32> availableBots = currentBots;

    uint32 availableBotCount = availableBots.size();
    uint32 onlineBotCount = playerBots.size();
    SetNextCheckDelay(10);

    if (availableBotCount < maxAllowedBotCount)
    {
        AddPVPBots();
    }

    // max Update 2?
    uint32 updateBots = 1;
    uint32 maxNewBots = onlineBotCount < maxAllowedBotCount ? maxAllowedBotCount - onlineBotCount : 0;
    uint32 loginBots = maxNewBots;

    /*LOG_INFO("server.loading", "have {} bots available", availableBots.size());
    LOG_INFO("server.loading", "have {} bots currently", currentBots.size());*/

    if (!availableBots.empty())
    {
        // Update some of the bots?
        for (auto bot : availableBots)
        {
            if (!GetPvpBot(bot)) {
                continue;
            }

            if (ProcessBot(bot))
            {
                updateBots--;
            }

            if (!updateBots) {
                break;
            }
        }

        if (loginBots)
        {
            loginBots += updateBots;

            LOG_INFO("playerbots", "{} new bots", loginBots);
            for (auto bot : availableBots)
            {
                if (GetPvpBot(bot)) {
                    continue;
                }

                if (ProcessBot(bot))
                {
                    loginBots--;
                }

                if (!loginBots) {
                    break;
                }
            }
        }
    }
}

void PvpBotMgr::GetBots()
{
    if (!currentBots.empty()) {
        return;
    }

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_PVP_BOTS_BY_OWNER_AND_EVENT);
    stmt->SetData(0, 0);
    stmt->SetData(1, "add");
    if (PreparedQueryResult result = PlayerbotsDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 bot = fields[0].Get<uint32>();
            if (GetEventValue(bot, "add")) {
                currentBots.push_back(bot);
            }
        }
        while (result->NextRow());
    }
}

void PvpBotMgr::CreatePvpBots()
{
    uint32 totalAccCount = 3;

    LOG_INFO("pvpbots", "Creating random bot accounts...");

    std::vector<std::future<void>> account_creations;
    bool account_creation = false;
    for (uint32 accountNumber = 0; accountNumber < totalAccCount; ++accountNumber)
    {
        std::ostringstream out;
        out << "PVPBot" << accountNumber;
        std::string const accountName = out.str();
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_GET_ACCOUNT_ID_BY_USERNAME);
        stmt->SetData(0, accountName);
        PreparedQueryResult result = LoginDatabase.Query(stmt);
        if (result)
        {
            continue;
        }
        account_creation = true;
        std::string password = "";
        for (int i = 0; i < 10; i++)
        {
            password += (char) urand('!', 'z');
        }

        AccountMgr::CreateAccount(accountName, password);

        LOG_DEBUG("pvpbots", "Account {} created for random bots", accountName.c_str());
    }

    if (account_creation) {
        /* wait for async accounts create to make character create correctly, same as account delete */
        std::this_thread::sleep_for(10ms * totalAccCount);
    }

    LOG_INFO("pvpbots", "Creating random bot characters...");
    uint32 totalPvpBotChars = 0;
    uint32 totalCharCount = totalAccCount * 3;

    std::unordered_map<uint8,std::vector<std::string>> names;
    //std::vector<std::pair<Player*, uint32>> playerBots;
    std::vector<WorldSession*> sessionBots;
    bool bot_creation = false;
    for (uint32 accountNumber = 0; accountNumber < totalAccCount; ++accountNumber)
    {
        std::ostringstream out;
        out << "PVPBot" << accountNumber;
        std::string const accountName = out.str();

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_GET_ACCOUNT_ID_BY_USERNAME);
        stmt->SetData(0, accountName);
        PreparedQueryResult result = LoginDatabase.Query(stmt);
        if (!result)
            continue;

        Field* fields = result->Fetch();
        uint32 accountId = fields[0].Get<uint32>();

        pvpBotAccounts.push_back(accountId);

        uint32 count = AccountMgr::GetCharactersCount(accountId);
        if (count >= 5)
        {
            continue;
        }
        bot_creation = true;
        LOG_INFO("pvpbots", "Creating random bot characters for account: [{}/{}]", accountNumber + 1, totalAccCount);
        RandomPlayerbotFactory factory(accountId);//, 80, ITEM_QUALITY_EPIC);

        WorldSession* session = new WorldSession(accountId, "", nullptr, SEC_PLAYER, EXPANSION_WRATH_OF_THE_LICH_KING, time_t(0), LOCALE_enUS, 0, false, false, 0, true);
        sessionBots.push_back(session);

        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES - count; ++cls)
        {
            // skip nonexistent classes
            if (!((1 << (cls - 1)) & CLASSMASK_ALL_PLAYABLE) || !sChrClassesStore.LookupEntry(cls))
                continue;

            if (bool const isClassDeathKnight = cls == CLASS_DEATH_KNIGHT;
                isClassDeathKnight &&
                sWorld->getIntConfig(CONFIG_EXPANSION) !=
                EXPANSION_WRATH_OF_THE_LICH_KING)
            {
                continue;
            }

            if (cls != 10)
                if (Player* pvpBot = factory.CreateRandomBot(session, cls)) {
                    pvpBot->SaveToDB(true, false);
                    sCharacterCache->AddCharacterCacheEntry(pvpBot->GetGUID(), accountId, pvpBot->GetName(),
                        pvpBot->getGender(), pvpBot->getRace(), pvpBot->getClass(), pvpBot->GetLevel());
                    pvpBot->CleanupsBeforeDelete();
                    delete pvpBot;
                }
        }
    }

    if (bot_creation) {
        LOG_INFO("pvpbots", "Waiting for {} characters loading into database...", totalCharCount);
        /* wait for characters load into database, or characters will fail to loggin */
        std::this_thread::sleep_for(5s);
    }

    for (WorldSession* session : sessionBots)
        delete session;

    for (uint32 accountId : pvpBotAccounts) {
        totalPvpBotChars += AccountMgr::GetCharactersCount(accountId);
    }
    LOG_INFO("server.loading", "{} random bot accounts with {} characters available", totalAccCount, totalPvpBotChars);
};

uint32 PvpBotMgr::AddPVPBots()
{
    uint32 maxAllowedBots = 1;
    if (currentBots.size() < maxAllowedBots)
    {

        for (std::vector<uint32>::iterator i = pvpBotAccounts.begin(); i != pvpBotAccounts.end(); i++)
        {
            uint32 accountId = *i;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID);
            stmt->SetData(0, accountId);
            PreparedQueryResult result = CharacterDatabase.Query(stmt);
            if (!result)
                continue;
            std::vector<uint32> guids;
            do
            {
                Field* fields = result->Fetch();
                ObjectGuid::LowType guid = fields[0].Get<uint32>();
                if (GetEventValue(guid, "add"))
                    continue;

                if (GetEventValue(guid, "logout"))
                    continue;

                if (GetPvpBot(guid))
                    continue;

                if (std::find(currentBots.begin(), currentBots.end(), guid) != currentBots.end())
                    continue;

                // Disable DKS? true for now i guess IDK
                if (true) {
                    QueryResult result = CharacterDatabase.Query("Select class from characters where guid = {}", guid);
                    if (!result) {
                        continue;
                    }
                    Field* fields = result->Fetch();
                    uint32 rClass = fields[0].Get<uint32>();
                    if (rClass == CLASS_DEATH_KNIGHT) {
                        continue;
                    }
                }
                guids.push_back(guid);
            } while (result->NextRow());

            std::mt19937 rnd(time(0));
            std::shuffle(guids.begin(), guids.end(), rnd);

            for (uint32 &guid : guids) {
                uint32 add_time = urand(360, 3600);

                SetEventValue(guid, "add", 1, add_time);
                SetEventValue(guid, "logout", 0, 0);
                currentBots.push_back(guid);

                maxAllowedBots--;
                if (!maxAllowedBots) {
                    break;
                }
            }

            if (!maxAllowedBots) {
                break;
            }
        }
    }
    return currentBots.size();
}

Player* PvpBotMgr::GetPvpBot(ObjectGuid playerGuid) const
{
    PvpPlayerBotMap::const_iterator it = playerBots.find(playerGuid);
    return (it == playerBots.end()) ? 0 : it->second;

}

Player* PvpBotMgr::GetPvpBot(ObjectGuid::LowType lowGuid) const
{
    ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(lowGuid);
    return GetPvpBot(playerGuid);

}

uint32 PvpBotMgr::GetEventValue(uint32 bot, std::string const event)
{
    // load all events at once on first event load
    if (eventCache[bot].empty())
    {
        PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_PVP_BOTS_BY_OWNER_AND_BOT);
        stmt->SetData(0, 0);
        stmt->SetData(1, bot);
        if (PreparedQueryResult result = PlayerbotsDatabase.Query(stmt))
        {
            do
            {
                Field* fields = result->Fetch();
                std::string const eventName = fields[0].Get<std::string>();

                CachedPvpEvent e;
                e.value = fields[1].Get<uint32>();
                e.lastChangeTime = fields[2].Get<uint32>();
                e.validIn = fields[3].Get<uint32>();
                e.data = fields[4].Get<std::string>();
                eventCache[bot][eventName] = std::move(e);
            }
            while (result->NextRow());
        }
    }

    CachedPvpEvent& e = eventCache[bot][event];
    /*if (e.IsEmpty())
    {
        QueryResult results = PlayerbotsDatabase.Query("SELECT `value`, `time`, validIn, `data` FROM playerbots_random_bots WHERE owner = 0 AND bot = {} AND event = {}",
                bot, event.c_str());

        if (results)
        {
            Field* fields = results->Fetch();
            e.value = fields[0].Get<uint32>();
            e.lastChangeTime = fields[1].Get<uint32>();
            e.validIn = fields[2].Get<uint32>();
            e.data = fields[3].Get<std::string>();
        }
    }
    */

    if ((time(0) - e.lastChangeTime) >= e.validIn && event != "specNo" && event != "specLink")
        e.value = 0;

    return e.value;
}

uint32 PvpBotMgr::SetEventValue(uint32 bot, std::string const event, uint32 value, uint32 validIn, std::string const data)
{
    PlayerbotsDatabaseTransaction trans = PlayerbotsDatabase.BeginTransaction();

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_DEL_PVP_BOTS_BY_OWNER_AND_EVENT);
    stmt->SetData(0, 0);
    stmt->SetData(1, bot);
    stmt->SetData(2, event.c_str());
    trans->Append(stmt);

    if (value)
    {
        stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_INS_PVP_BOTS);
        stmt->SetData(0, 0);
        stmt->SetData(1, bot);
        stmt->SetData(2, static_cast<uint32>(GameTime::GetGameTime().count()));
        stmt->SetData(3, validIn);
        stmt->SetData(4, event.c_str());
        stmt->SetData(5, value);
        if (data != "")
        {
            stmt->SetData(6, data.c_str());
        }
        else
        {
            stmt->SetData(6);
        }
        trans->Append(stmt);
    }

    PlayerbotsDatabase.CommitTransaction(trans);

    CachedPvpEvent e(value, (uint32)time(nullptr), validIn, data);
    eventCache[bot][event] = std::move(e);
    return value;
}

bool PvpBotMgr::ProcessBot(uint32 bot)
{
    ObjectGuid botGUID = ObjectGuid::Create<HighGuid::Player>(bot);

    Player* player = GetPvpBot(botGUID);
    PlayerbotAI* botAI = player ? GET_PVPPLAYERBOT_AI(player) : nullptr;

    uint32 isValid = GetEventValue(bot, "add");
    if (!isValid)
    {
		if (!player || !player->GetGroup())
		{
            if (player)
                LOG_INFO("pvpbots", "Bot #{} {}:{} <{}>: log out", bot, IsAlliance(player->getRace()) ? "A" : "H", player->GetLevel(), player->GetName().c_str());
            else
                LOG_INFO("pvpbots", "Bot #{}: log out", bot);

			SetEventValue(bot, "add", 0, 0);
			currentBots.erase(std::remove(currentBots.begin(), currentBots.end(), bot), currentBots.end());

			if (player)
                LogoutPlayerBot(botGUID);
		}

        return false;
    }

    uint32 isLogginIn = GetEventValue(bot, "login");
    if (isLogginIn)
        return false;

    if (!player)
    {
        AddPlayerBot(botGUID, 0);
        SetEventValue(bot, "login", 1, 1);

        // TODO min and maxRandomBotReviveTime
        uint32 randomTime = urand(60, 100);
        SetEventValue(bot, "update", 1, randomTime);

        // do not randomize or teleport immediately after server start (prevent lagging)
        if (!GetEventValue(bot, "randomize")) {
            // TODO randomBotUpdateInterval
            randomTime = urand(20 * 5, 20 * 20);
            ScheduleRandomize(bot, randomTime);
        }

        if (!GetEventValue(bot, "teleport")) {
            // TODO randomBotUpdateInterval
            randomTime = urand(20 * 2, 20 * 10);
            std::cout << "setting teleport: " << randomTime << "\n";
            ScheduleTeleport(bot, randomTime);
        }
        return true;
    }

    SetEventValue(bot, "login", 0, 0);

    if (player->GetGroup() || player->HasUnitState(UNIT_STATE_IN_FLIGHT))
        return false;

    uint32 update = GetEventValue(bot, "update");
    if (!update)
    {
        if (botAI)
            // TODO pvp update?
            botAI->GetAiObjectContext()->GetValue<bool>("random bot update")->Set(true);

        bool update = true;
        if (botAI)
        {
            //botAI->GetAiObjectContext()->GetValue<bool>("random bot update")->Set(true);
            // TODO IsPvpBot method
            /*if (!sPvpBotMgr->IsRandomBot(player))*/
            if (player->GetGroup() && botAI->GetGroupMaster())
            {
                PlayerbotAI* groupMasterBotAI = GET_PVPPLAYERBOT_AI(botAI->GetGroupMaster());
                if (!groupMasterBotAI || groupMasterBotAI->IsRealPlayer())
                {
                    update = false;
                }
            }

            // if (botAI->HasPlayerNearby(sPlayerbotAIConfig->grindDistance))
            //     update = false;
        }

        if (update) {
            std::cout << "Updating Bot\n";
            ProcessBot(player);
        }

        // TODO min / maxRandomBotReviveTime
        uint32 randomTime = urand(60, 100);
        SetEventValue(bot, "update", 1, randomTime);

        return true;
    }

    uint32 logout = GetEventValue(bot, "logout");
    if (player && !logout && !isValid)
    {
        LOG_INFO("playerbots", "Bot #{} {}:{} <{}>: log out", bot, IsAlliance(player->getRace()) ? "A" : "H", player->GetLevel(), player->GetName().c_str());
        LogoutPlayerBot(botGUID);
        currentBots.remove(bot);
        SetEventValue(bot, "logout", 1, urand(30 * MINUTE, 1 * HOUR));
        return true;
    }

    return false;
}

bool PvpBotMgr::ProcessBot(Player* player)
{
    uint32 bot = player->GetGUID().GetCounter();
    std::cout << "Process bot: " << player->GetName() << "\n";

    if (player->InBattleground())
        return false;

    if (player->InBattlegroundQueue())
        return false;

    //if (player->isDead())
        //return false;
    // TODO Handle is in raid event -> logout
    if (player->isDead())
    {
        if (!GetEventValue(bot, "dead"))
        {
            // TODO minRandomBotReviveTime
            uint32 randomTime = urand(60, 100);
            // LOG_INFO("playerbots", "Mark bot {} as dead, will be revived in {}s.", player->GetName().c_str(), randomTime);
            SetEventValue(bot, "dead", 1, 1 * HOUR);
            SetEventValue(bot, "revive", 1, randomTime);
            return false;
        }

        if (!GetEventValue(bot, "revive"))
        {
			Revive(player);
			return true;
        }

        return false;
    }

    // TODO Cleanup group?
    /*Group* group = player->GetGroup();
    if (group && !group->isLFGGroup() && IsRandomBot(group->GetLeader())) {
        player->RemoveFromGroup();
        LOG_INFO("playerbots", "Bot {} remove from group since leader is random bot.", player->GetName().c_str());
    }*/

    //GET_PLAYERBOT_AI(player)->GetAiObjectContext()->GetValue<bool>("random bot update")->Set(false);

    // bool randomiser = true;
    // if (player->GetGuildId())
    // {
    //     if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
    //     {
    //         if (guild->GetLeaderGUID() == player->GetGUID())
    //         {
    //             for (std::vector<Player*>::iterator i = players.begin(); i != players.end(); ++i)
    //                 sGuildTaskMgr->Update(*i, player);
    //         }

    //         uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(guild->GetLeaderGUID());
    //         if (!sPlayerbotAIConfig->IsInRandomAccountList(accountId))
    //         {
    //             uint8 rank = player->GetRank();
    //             randomiser = rank < 4 ? false : true;
    //         }
    //     }
    // }

    uint32 randomize = GetEventValue(bot, "randomize");
    if (!randomize)
    {
        std::cout << "Randomizing\n";
        Randomize(player);
        std::cout << "Refreshing from process\n";
        Refresh(player);
        LOG_INFO("playerbots", "Bot #{} {}:{} <{}>: randomized", bot, player->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", player->GetLevel(), player->GetName());
        // TODO schedule new randomize?
        /*uint32 randomTime = urand(sPlayerbotAIConfig->minRandomBotRandomizeTime, sPlayerbotAIConfig->maxRandomBotRandomizeTime);
        ScheduleRandomize(bot, randomTime);*/
        return true;
    }

    // enable random teleport logic if no auto traveling enabled
    // if (!sPlayerbotAIConfig->autoDoQuests)
    // {
    // TODO handle telporting bots around
    uint32 teleport = GetEventValue(bot, "teleport");
    if (teleport)
    {
        std::cout << "teleporting?\n";
        LOG_INFO("pvpbots", "Bot #{} <{}>: teleport for level and refresh", bot, player->GetName());
        RandomTeleportForLevel(player);
        //uint32 time = urand(sPlayerbotAIConfig->minRandomBotTeleportInterval, sPlayerbotAIConfig->maxRandomBotTeleportInterval);
        uint32 time = urand(60, 360);
        ScheduleTeleport(bot, time);
        return true;
    }
    // }

    // uint32 changeStrategy = GetEventValue(bot, "change_strategy");
    // if (!changeStrategy)
    // {
    //     LOG_INFO("playerbots", "Changing strategy for bot  #{} <{}>", bot, player->GetName().c_str());
    //     ChangeStrategy(player);
    //     return true;
    // }

    return false;
}


void PvpBotMgr::RandomTeleportForLevel(Player* bot)
{
    if (bot->InBattleground())
        return;

    uint32 level = bot->GetLevel();
    uint8 race = bot->getRace();
    LOG_DEBUG("pvpbots", "Random teleporting bot {} for level {} ({} locations available)", bot->GetName().c_str(),
              bot->GetLevel(), locsPerRaidCache[level].size());
    /*
    if (level > 10 && urand(0, 100) < sPlayerbotAIConfig->probTeleToBankers * 100)
    {
        RandomTeleport(bot, bankerLocsPerLevelCache[level], true);
    }*/
    if (bot->GetTeamId() == TEAM_ALLIANCE) {
        RandomTeleport(bot, locsPerRaidCache[1]);
    } else {
        RandomTeleport(bot, locsPerRaidCache[3]);
    }
}

void PvpBotMgr::RandomTeleport(Player* bot)
{
    if (bot->InBattleground())
        return;

    std::vector<WorldLocation> locs;

    std::list<Unit*> targets;
    float range = sPlayerbotAIConfig->randomBotTeleportDistance;
    Acore::AnyUnitInObjectRangeCheck u_check(bot, range);
    Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
    Cell::VisitAllObjects(bot, searcher, range);

    if (!targets.empty())
    {
        for (Unit* unit : targets)
        {
            bot->UpdatePosition(*unit);
            FleeManager manager(bot, sPlayerbotAIConfig->sightDistance, 0, true);
            float rx, ry, rz;
            if (manager.CalculateDestination(&rx, &ry, &rz))
            {
                WorldLocation loc(bot->GetMapId(), rx, ry, rz);
                locs.push_back(loc);
            }
        }
    }
    else
    {
        RandomTeleportForLevel(bot);
    }
    std::cout << "Refreshing from teleport\n";
    Refresh(bot);
}

void PvpBotMgr::RandomTeleport(Player* bot, std::vector<WorldLocation>& locs, bool hearth)
{
    if (bot->IsBeingTeleported())
        return;

    if (bot->InBattleground())
        return;

    if (bot->InBattlegroundQueue())
        return;

    // if (bot->GetLevel() < 5)
    //     return;

    // if (sPlayerbotAIConfig->randomBotRpgChance < 0)
    //     return;

    if (locs.empty())
    {
        std::cout << "no locs\n";
        LOG_DEBUG("playerbots", "Cannot teleport bot {} - no locations available", bot->GetName().c_str());
        return;
    }

    std::vector<WorldPosition> tlocs;
    for (auto& loc : locs)
        tlocs.push_back(WorldPosition(loc));
    // Do not teleport to maps disabled in config
    tlocs.erase(std::remove_if(tlocs.begin(), tlocs.end(),
                               [bot](WorldPosition l)
                               {
                                   std::vector<uint32>::iterator i =
                                       find(sPlayerbotAIConfig->randomBotMaps.begin(),
                                            sPlayerbotAIConfig->randomBotMaps.end(), l.getMapId());
                                   return i == sPlayerbotAIConfig->randomBotMaps.end();
                               }),
                tlocs.end());
    if (tlocs.empty())
    {
        LOG_DEBUG("playerbots", "Cannot teleport bot {} - all locations removed by filter", bot->GetName().c_str());
        return;
    }

    if (tlocs.empty())
    {
        LOG_DEBUG("playerbots", "Cannot teleport bot {} - no locations available", bot->GetName().c_str());
        return;
    }

    std::shuffle(std::begin(tlocs), std::end(tlocs), RandomEngine::Instance());
    for (uint32 i = 0; i < tlocs.size(); i++)
    {
        WorldLocation loc = tlocs[i];

        float x = loc.GetPositionX();  // + (attemtps > 0 ? urand(0, sPlayerbotAIConfig->grindDistance) -
                                       // sPlayerbotAIConfig->grindDistance / 2 : 0);
        float y = loc.GetPositionY();  // + (attemtps > 0 ? urand(0, sPlayerbotAIConfig->grindDistance) -
                                       // sPlayerbotAIConfig->grindDistance / 2 : 0);
        float z = loc.GetPositionZ();

        Map* map = sMapMgr->FindMap(loc.GetMapId(), 0);
        if (!map)
            continue;

        AreaTableEntry const* zone = sAreaTableStore.LookupEntry(map->GetZoneId(bot->GetPhaseMask(), x, y, z));
        if (!zone)
            continue;

        AreaTableEntry const* area = sAreaTableStore.LookupEntry(map->GetAreaId(bot->GetPhaseMask(), x, y, z));
        if (!area)
            continue;

        // Do not teleport to enemy zones if level is low
        if (zone->team == 4 && bot->GetTeamId() == TEAM_ALLIANCE)
            continue;

        if (zone->team == 2 && bot->GetTeamId() == TEAM_HORDE)
            continue;

        if (map->IsInWater(bot->GetPhaseMask(), x, y, z, bot->GetCollisionHeight()))
            continue;

        float ground = map->GetHeight(bot->GetPhaseMask(), x, y, z + 0.5f);
        if (ground <= INVALID_HEIGHT)
            continue;

        z = 0.05f + ground;
        PlayerInfo const* pInfo = sObjectMgr->GetPlayerInfo(bot->getRace(true), bot->getClass());
        float dis = loc.GetExactDist(pInfo->positionX, pInfo->positionY, pInfo->positionZ);
        // yunfan: distance check for low level
        if (bot->GetLevel() <= 4 && (loc.GetMapId() != pInfo->mapId || dis > 500.0f))
        {
            continue;
        }
        if (bot->GetLevel() <= 10 && (loc.GetMapId() != pInfo->mapId || dis > 2500.0f))
        {
            continue;
        }
        if (bot->GetLevel() <= 18 && (loc.GetMapId() != pInfo->mapId || dis > 10000.0f))
        {
            continue;
        }

        const LocaleConstant& locale = sWorld->GetDefaultDbcLocale();
        LOG_INFO("playerbots",
                 "Random teleporting bot {} (level {}) to Map: {} ({}) Zone: {} ({}) Area: {} ({}) {},{},{} ({}/{} "
                 "locations)",
                 bot->GetName().c_str(), bot->GetLevel(), map->GetId(), map->GetMapName(), zone->ID,
                 zone->area_name[locale], area->ID, area->area_name[locale], x, y, z, i + 1, tlocs.size());

        if (hearth)
        {
            bot->SetHomebind(loc, zone->ID);
        }

        std::cout << "I'm here\n";
        bot->GetMotionMaster()->Clear();
        PlayerbotAI* botAI = GET_PVPPLAYERBOT_AI(bot);
        std::cout << "I'm here2\n";
        if (botAI)
            botAI->Reset(true);
        std::cout << "I'm here3\n";
        bot->TeleportTo(loc.GetMapId(), x, y, z, 0);
        bot->SendMovementFlagUpdate();

        std::cout << "I'm here4\n";
        return;
    }

    LOG_ERROR("playerbots", "Cannot teleport bot {} - no locations available ({} locations)", bot->GetName().c_str(),
              tlocs.size());
}


void PvpBotMgr::ScheduleRandomize(uint32 bot, uint32 time)
{
    SetEventValue(bot, "randomize", 1, time);
}
void PvpBotMgr::ScheduleTeleport(uint32 bot, uint32 time)
{
    if (!time)
        time = 60 + urand(sPlayerbotAIConfig->randomBotUpdateInterval, sPlayerbotAIConfig->randomBotUpdateInterval * 3);

    SetEventValue(bot, "teleport", 1, time);

}
void PvpBotMgr::ScheduleChangeStrategy(uint32 bot, uint32 time){
    if (!time)
        time = urand(sPlayerbotAIConfig->minRandomBotChangeStrategyTime, sPlayerbotAIConfig->maxRandomBotChangeStrategyTime);

    SetEventValue(bot, "change_strategy", 1, time);
}


void PvpBotMgr::OnBotLoginInternal(Player * const bot)
{
    LOG_INFO("pvpbots", "{}/{} Bot {} logged in", pvpBots.size(), 40, bot->GetName().c_str());
}

void PvpBotMgr::Randomize(Player* bot)
{
    if (bot->InBattleground())
        return;

    if (bot->GetLevel() < 3 || (bot->GetLevel() < 56 && bot->getClass() == CLASS_DEATH_KNIGHT)) {
        RandomizeFirst(bot);
    }
    else if (bot->GetLevel() < sPlayerbotAIConfig->randomBotMaxLevel || !sPlayerbotAIConfig->downgradeMaxLevelBot) {
        uint8 level = bot->GetLevel();
        PlayerbotFactory factory(bot, level);
        factory.Randomize(true);
    }
    else {
        RandomizeFirst(bot);
    }
    // TODO teleport to PVP event
    //RandomTeleportForLevel(bot);
}

void PvpBotMgr::RandomizeFirst(Player* bot)
{
	uint32 maxLevel = sPlayerbotAIConfig->randomBotMaxLevel;
	if (maxLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
		maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);

    // if lvl sync is enabled, max level is limited by online players lvl
    /*if (sPlayerbotAIConfig->syncLevelWithPlayers)
        maxLevel = std::max(sPlayerbotAIConfig->randomBotMinLevel, std::min(playersLevel, sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)));*/

	//PerformanceMonitorOperation* pmo = sPerformanceMonitor->start(PERF_MON_RNDBOT, "RandomizeFirst");

    // TODO configure PVP char level
    uint32 level = maxLevel;


    // Random Levels enabled?
    /*if (false) {
        level = bot->getClass() == CLASS_DEATH_KNIGHT ?
            std::max(sPlayerbotAIConfig->randombotStartingLevel, sWorld->getIntConfig(CONFIG_START_HEROIC_PLAYER_LEVEL)) :
            sPlayerbotAIConfig->randombotStartingLevel;
    }*/

    SetValue(bot, "level", level);

    PlayerbotFactory factory(bot, level);
    factory.Randomize(false);

    // TODO minRandomBotRandomizeTime
    uint32 randomTime = urand(3 * HOUR, 8 * HOUR);
    // TODO inWorldTime
    uint32 inworldTime = urand(2 * DAY, 1 * WEEK);

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_UPD_PVP_BOTS);
    stmt->SetData(0, randomTime);
    stmt->SetData(1, "bot_delete");
    stmt->SetData(2, bot->GetGUID().GetCounter());
    PlayerbotsDatabase.Execute(stmt);

    stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_UPD_PVP_BOTS);
    stmt->SetData(0, inworldTime);
    stmt->SetData(1, "logout");
    stmt->SetData(2, bot->GetGUID().GetCounter());
    PlayerbotsDatabase.Execute(stmt);

    // teleport to a random inn for bot level
    if (GET_PVPPLAYERBOT_AI(bot)) {
        GET_PVPPLAYERBOT_AI(bot)->Reset(true);
        GET_PVPPLAYERBOT_AI(bot)->ResetStrategies2();
    }


    if (bot->GetGroup())
        bot->RemoveFromGroup();
}

void PvpBotMgr::RandomizeMin(Player* bot)
{
	//PerformanceMonitorOperation* pmo = sPerformanceMonitor->start(PERF_MON_RNDBOT, "RandomizeMin");

    // TODO wut?
    uint32 level = 37; //sPlayerbotAIConfig->randomBotMinLevel;

    SetValue(bot, "level", level);

    PlayerbotFactory factory(bot, level);
    factory.Randomize(false);

    // TODO minRandomBotRandomizeTime
    uint32 randomTime = urand(3 * HOUR, 8 * HOUR);
    // TODO inWorldTime
    uint32 inworldTime = urand(2 * DAY, 1 * WEEK);

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_UPD_PVP_BOTS);
    stmt->SetData(0, randomTime);
    stmt->SetData(1, "bot_delete");
    stmt->SetData(2, bot->GetGUID().GetCounter());
    PlayerbotsDatabase.Execute(stmt);

    stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_UPD_PVP_BOTS);
    stmt->SetData(0, inworldTime);
    stmt->SetData(1, "logout");
    stmt->SetData(2, bot->GetGUID().GetCounter());
    PlayerbotsDatabase.Execute(stmt);

    // teleport to a random inn for bot level
    if (GET_PVPPLAYERBOT_AI(bot))
        GET_PVPPLAYERBOT_AI(bot)->Reset(true);

    if (bot->GetGroup())
        bot->RemoveFromGroup();

	/*if (pmo)
        pmo->finish();*/
}

void PvpBotMgr::OnPlayerLogin(Player* player)
{
    std::cout << "OnPlayerLogin\n";
    uint32 botsNearby = 0;

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (player == bot /* || GET_PVPPLAYERBOT_AI(player)*/)  // TEST
            continue;

        Cell playerCell(player->GetPositionX(), player->GetPositionY());
        Cell botCell(bot->GetPositionX(), bot->GetPositionY());

        // if (playerCell == botCell)
        // botsNearby++;

        Group* group = bot->GetGroup();
        if (!group)
            continue;

        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            PlayerbotAI* botAI = GET_PVPPLAYERBOT_AI(bot);
            if (botAI && member == player && (!botAI->GetMaster() || GET_PVPPLAYERBOT_AI(botAI->GetMaster())))
            {
                if (!bot->InBattleground())
                {
                    botAI->SetMaster(player);
                    botAI->ResetStrategies2();
                    botAI->TellMaster("Hello");
                }

                break;
            }
        }
    }

    if (botsNearby > 100 && false)
    {
        WorldPosition botPos(player);

        // botPos.GetReachableRandomPointOnGround(player, sPlayerbotAIConfig->reactDistance * 2, true);

        // player->TeleportTo(botPos);
        // player->Relocate(botPos.coord_x, botPos.coord_y, botPos.coord_z, botPos.orientation);

        if (!player->GetFactionTemplateEntry())
        {
            botPos.GetReachableRandomPointOnGround(player, sPlayerbotAIConfig->reactDistance * 2, true);
        }
        else
        {
            std::vector<TravelDestination*> dests = sTravelMgr->getRpgTravelDestinations(player, true, true, 200000.0f);

            do
            {
                RpgTravelDestination* dest = (RpgTravelDestination*)dests[urand(0, dests.size() - 1)];
                CreatureTemplate const* cInfo = dest->GetCreatureTemplate();
                if (!cInfo)
                    continue;

                FactionTemplateEntry const* factionEntry = sFactionTemplateStore.LookupEntry(cInfo->faction);
                ReputationRank reaction = Unit::GetFactionReactionTo(player->GetFactionTemplateEntry(), factionEntry);

                if (reaction > REP_NEUTRAL && dest->nearestPoint(&botPos)->m_mapId == player->GetMapId())
                {
                    botPos = *dest->nearestPoint(&botPos);
                    break;
                }
            } while (true);
        }

        player->TeleportTo(botPos);

        // player->Relocate(botPos.getX(), botPos.getY(), botPos.getZ(), botPos.getO());
    }

    if (IsPvpBot(player))
    {
        ObjectGuid::LowType guid = player->GetGUID().GetCounter();
        SetEventValue(guid, "login", 0, 0);
    }
    /*else
    {
        players.push_back(player);
        LOG_DEBUG("playerbots", "Including non-random bot player {} into random bot update", player->GetName().c_str());
    }*/
}

bool PvpBotMgr::IsPvpBot(Player* bot)
{
    if (bot && GET_PVPPLAYERBOT_AI(bot))
    {
        if (GET_PVPPLAYERBOT_AI(bot)->IsRealPlayer())
            return false;
    }
    if (bot)
    {
        return IsPvpBot(bot->GetGUID().GetCounter());
    }

}

bool PvpBotMgr::IsPvpBot(ObjectGuid::LowType bot)
{
    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(bot);
    if (std::find(currentBots.begin(), currentBots.end(), bot) != currentBots.end())
        return true;
    return false;
}

void PvpBotMgr::Clear(Player* bot)
{
    PlayerbotFactory factory(bot, bot->GetLevel());
    factory.ClearEverything();
}

/*uint32 PvpBotMgr::GetZoneLevel(uint16 mapId, float teleX, float teleY, float teleZ)
{
	uint32 maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);

    uint32 level = 0;
    QueryResult results = WorldDatabase.Query("SELECT AVG(t.minlevel) minlevel, AVG(t.maxlevel) maxlevel FROM creature c "
            "INNER JOIN creature_template t ON c.id1 = t.entry WHERE map = {} AND minlevel > 1 AND ABS(position_x - {}) < {} AND ABS(position_y - {}) < {}",
            mapId, teleX, sPlayerbotAIConfig->randomBotTeleportDistance / 2, teleY, sPlayerbotAIConfig->randomBotTeleportDistance / 2);

    if (results)
    {
        Field* fields = results->Fetch();
        uint8 minLevel = fields[0].Get<uint8>();
        uint8 maxLevel = fields[1].Get<uint8>();
        level = urand(minLevel, maxLevel);
        if (level > maxLevel)
            level = maxLevel;
    }
    else
    {
        level = urand(1, maxLevel);
    }

    return level;
}*/

void PvpBotMgr::Revive(Player* player)
{
    uint32 bot = player->GetGUID().GetCounter();

    // LOG_INFO("playerbots", "Bot {} revived", player->GetName().c_str());
    SetEventValue(bot, "dead", 0, 0);
    SetEventValue(bot, "revive", 0, 0);
}

void PvpBotMgr::Refresh(Player* bot)
{
    std::cout << "Refreshing " << bot->GetName() << "\n";
    PlayerbotAI* botAI = GET_PVPPLAYERBOT_AI(bot);
    if (!botAI) {
        std::cout << "No botAI :(\n";
        return;
    }
    std::cout << "Bot Active?: " << botAI->IsActive() << "\n";

    if (bot->isDead())
    {
        bot->ResurrectPlayer(1.0f);
        bot->SpawnCorpseBones();
        botAI->ResetStrategies2(false);
    }

    // if (sPlayerbotAIConfig->disableRandomLevels)
    //     return;

    if (bot->InBattleground())
        return;

    LOG_DEBUG("playerbots", "Refreshing bot {} <{}>", bot->GetGUID().ToString().c_str(), bot->GetName().c_str());

    /*PerformanceMonitorOperation* pmo = sPerformanceMonitor->start(PERF_MON_RNDBOT, "Refresh");*/

    botAI->Reset();

    bot->DurabilityRepairAll(false, 1.0f, false);
	bot->SetFullHealth();
	bot->SetPvP(true);

    PlayerbotFactory factory(bot, bot->GetLevel(), ITEM_QUALITY_RARE);
    factory.Refresh();

    if (bot->GetMaxPower(POWER_MANA) > 0)
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));

    if (bot->GetMaxPower(POWER_ENERGY) > 0)
        bot->SetPower(POWER_ENERGY, bot->GetMaxPower(POWER_ENERGY));

    // TODO cleanup and logout
    if (bot->GetGroup())
        bot->RemoveFromGroup();


    /*if (pmo)
        pmo->finish();*/
}

uint32 PvpBotMgr::GetValue(uint32 bot, std::string const type)
{
    return GetEventValue(bot, type);
}

uint32 PvpBotMgr::GetValue(Player* bot, std::string const type)
{
    return GetValue(bot->GetGUID().GetCounter(), type);
}

void PvpBotMgr::SetValue(uint32 bot, std::string const type, uint32 value, std::string const data)
{
    SetEventValue(bot, type, value, sPlayerbotAIConfig->maxRandomBotInWorldTime, data);
}

void PvpBotMgr::SetValue(Player* bot, std::string const type, uint32 value, std::string const data)
{
    SetValue(bot->GetGUID().GetCounter(), type, value, data);
}
