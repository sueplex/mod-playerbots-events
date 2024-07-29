

#include "PvpBotMgr.h"
#include "AccountMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Player.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotFactory.h"

#include <vector>
#include <stdint.h>

bool PvpBotMgr::UpdateAIInternal(uint32 elapsed, bool minimal = false)
{
    uint32 totalAccCount = 80;

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
    uint32 totalCharCount = totalAccCount * 10;

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
        if (count >= 10)
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
                if (Player* pvpBot = factory.CreateRandomBot(session, cls, names)) {
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
    return true;
};

uint32 PvpBotMgr::AddPVPBots()
{
    uint32 maxAllowedBots = 40;
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

                if (GetPlayerBot(guid))
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

            /*std::mt19937 rnd(time(0));
            std::shuffle(guids.begin(), guids.end(), rnd);*/

            for (uint32 &guid : guids) {
                uint32 add_time = 1 * HOUR;

                SetEventValue(guid, "add", 1, add_time);
                SetEventValue(guid, "logout", 0, 0);
                currentBots.push_back(guid);

                maxAllowedBots--;
                if (!maxAllowedBots)
                    break;
            }

            if (!maxAllowedBots)
                break;
        }
    }
}

uint32 PvpBotMgr::GetEventValue(uint32 bot, std::string const event)
{
    // load all events at once on first event load
    if (eventCache[bot].empty())
    {
        PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_BOT);
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

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_DEL_RANDOM_BOTS_BY_OWNER_AND_EVENT);
    stmt->SetData(0, 0);
    stmt->SetData(1, bot);
    stmt->SetData(2, event.c_str());
    trans->Append(stmt);

    if (value)
    {
        stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_INS_RANDOM_BOTS);
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

void PvpBotMgr::OnBotLoginInternal(Player * const bot)
{
    LOG_INFO("playerbots", "{}/{} Bot {} logged in", pvpBots.size(), 40, bot->GetName().c_str());
}
