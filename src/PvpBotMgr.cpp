

#include "PvpBotMgr.h"
#include "AccountMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotFactory.h"

#include <cstdlib>
#include <iostream>
#include <random>
#include <stdint.h>
#include <vector>

PvpBotMgr::PvpBotMgr() : PlayerbotHolder() {

}

PvpBotMgr::~PvpBotMgr() {

}

void PvpBotMgr::UpdateAIInternal(uint32 elapsed, bool minimal)
{
    std::cout << "Updating internal Pvpbotmgr\n";
    // Get and Set Value for bot_count here?
    uint32 maxAllowedBotCount = 2;

    GetBots();
    std::cout << "got bots\n";
    std::list<uint32> availableBots = currentBots;

    uint32 availableBotCount = availableBots.size();
    uint32 onlineBotCount = pvpBots.size();
    // SetNextCheckDelay?

    if (availableBotCount < maxAllowedBotCount)
    {
        AddPVPBots();
    }

    // max Update 2?
    uint32 updateBots = 2;
    uint32 maxNewBots = onlineBotCount < maxAllowedBotCount ? maxAllowedBotCount - onlineBotCount : 0;
    uint32 loginBots = maxNewBots;

    LOG_INFO("server.loading", "have {} bots available", availableBots.size());
    LOG_INFO("server.loading", "have {} bots currently", currentBots.size());

    if (!availableBots.empty())
    {
        // Update some of the bots?
        for (auto bot : availableBots)
        {
            if (!GetPvpBot(bot))
                continue;
            std::cout << "processing a bot\n";
            if (ProcessBot(bot))
            {
                updateBots--;
            }

            if (!updateBots)
                std::cout << "breaking from update bots\n";
                break;
        }

        if (loginBots)
        {
            loginBots += updateBots;

            LOG_INFO("playerbots", "{} new bots", loginBots);
            for (auto bot : availableBots)
            {
                if (GetPvpBot(bot))
                    continue;

                std::cout << "logging in a bot\n";
                if (ProcessBot(bot))
                {
                    loginBots--;
                }

                if (!loginBots)
                    std::cout << "breaking from loginbots\n";
                    break;
            }
        }
    }
}

void PvpBotMgr::GetBots()
{
    if (!currentBots.empty())
        return;

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_PVP_BOTS_BY_OWNER_AND_EVENT);
    stmt->SetData(0, 0);
    stmt->SetData(1, "add");
    if (PreparedQueryResult result = PlayerbotsDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 bot = fields[0].Get<uint32>();
            if (GetEventValue(bot, "add"))
                std::cout << "pushing bot\n";
                currentBots.push_back(bot);
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
        std::this_thread::sleep_for(10s);
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
    uint32 maxAllowedBots = 2;
    if (currentBots.size() < maxAllowedBots)
    {

        for (std::vector<uint32>::iterator i = pvpBotAccounts.begin(); i != pvpBotAccounts.end(); i++)
        {
            uint32 accountId = *i;
            std::cout << "adding for " << accountId << "\n";

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
                std::cout << "pushing bot guid\n";
                guids.push_back(guid);
            } while (result->NextRow());

            std::mt19937 rnd(time(0));
            std::shuffle(guids.begin(), guids.end(), rnd);

            for (uint32 &guid : guids) {
                uint32 add_time = 0;
                std::cout << "setting add and logout\n";

                SetEventValue(guid, "add", 1, add_time);
                SetEventValue(guid, "logout", 1, 900);
                currentBots.push_back(guid);

                maxAllowedBots--;
                if (!maxAllowedBots) {
                    std::cout << "breaking for account\n";
                    break;
                }
            }

            if (!maxAllowedBots) {
                std::cout << "breaking for add bots\n";
                break;
            }
        }
    }
    return currentBots.size();
}

Player* PvpBotMgr::GetPvpBot(ObejctGuid playerGuid) const
{
    PlayerBotMap::const_iterator it = playerBots.find(playerGuid);
    return (it == playerBots.end()) ? 0 : it->second;

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
    PlayerbotAI* botAI = player ? GET_PLAYERBOT_AI(player) : nullptr;

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
            randomTime = urand(20 * 5, 20 * 20);
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
            if (std::find(currentBots.begin(), currentBots.end(), bot) != currentBots.end())
                update = false;

            if (player->GetGroup() && botAI->GetGroupMaster())
            {
                PlayerbotAI* groupMasterBotAI = GET_PLAYERBOT_AI(botAI->GetGroupMaster());
                if (!groupMasterBotAI || groupMasterBotAI->IsRealPlayer())
                {
                    update = false;
                }
            }

            // if (botAI->HasPlayerNearby(sPlayerbotAIConfig->grindDistance))
            //     update = false;
        }

        if (update)
            ProcessBot(player);

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
        Randomize(player);
        LOG_INFO("playerbots", "Bot #{} {}:{} <{}>: randomized", bot, player->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", player->GetLevel(), player->GetName());
        // TODO schedule new randomize?
        /*uint32 randomTime = urand(sPlayerbotAIConfig->minRandomBotRandomizeTime, sPlayerbotAIConfig->maxRandomBotRandomizeTime);
        ScheduleRandomize(bot, randomTime);*/
        return true;
    }

    // enable random teleport logic if no auto traveling enabled
    // if (!sPlayerbotAIConfig->autoDoQuests)
    // {
    /*  TODO handle telporting bots around
    uint32 teleport = GetEventValue(bot, "teleport");
    if (!teleport)
    {
        LOG_INFO("playerbots", "Bot #{} <{}>: teleport for level and refresh", bot, player->GetName());
        Refresh(player);
        RandomTeleportForLevel(player);
        uint32 time = urand(sPlayerbotAIConfig->minRandomBotTeleportInterval, sPlayerbotAIConfig->maxRandomBotTeleportInterval);
        ScheduleTeleport(bot, time);
        return true;
    }*/
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
    LOG_INFO("playerbots", "{}/{} Bot {} logged in", pvpBots.size(), 40, bot->GetName().c_str());
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
        // IncreaseLevel(bot);
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
    if (GET_PLAYERBOT_AI(bot))
        GET_PLAYERBOT_AI(bot)->Reset(true);

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
    if (GET_PLAYERBOT_AI(bot))
        GET_PLAYERBOT_AI(bot)->Reset(true);

    if (bot->GetGroup())
        bot->RemoveFromGroup();

	/*if (pmo)
        pmo->finish();*/
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


    // TODO handle the revive here? what should they do
    /*
    Refresh(player);
    RandomTeleportGrindForLevel(player);
    */
}

void PvpBotMgr::Refresh(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return;

    if (bot->isDead())
    {
        bot->ResurrectPlayer(1.0f);
        bot->SpawnCorpseBones();
        botAI->ResetStrategies(false);
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
