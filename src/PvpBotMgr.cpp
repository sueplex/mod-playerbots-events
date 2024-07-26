

#include "PvpBotMgr.h"
#include "AccountMgr.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "RandomPlayerBotFactory.h"

#include <vector>
#include <stdint.h>

bool PvpBotMgr::Initialize()
    uint32 totalAccCount = 80;

    LOG_INFO("pvpbots", "Creating random bot accounts...");

    std::vector<std::future<void>> account_creations;
    bool account_creation = false;
    for (uint32 accountNumber = 0; accountNumber < 80; ++accountNumber)
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
        std::this_thread::sleep_for(10ms * 80);
    }

    LOG_INFO("pvpbots", "Creating random bot characters...");
    uint32 totalPvpBotChars = 0;
    uint32 totalCharCount = 80 * 10;

    std::unordered_map<uint8,std::vector<std::string>> names;
    //std::vector<std::pair<Player*, uint32>> playerBots;
    std::vector<WorldSession*> sessionBots;
    bool bot_creation = false;
    for (uint32 accountNumber = 0; accountNumber < 80; ++accountNumber)
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
        LOG_INFO("pvpbots", "Creating random bot characters for account: [{}/{}]", accountNumber + 1, 80);
        RandomPlayerbotFactory factory(accountId, 80, ITEM_QUALITY_EPIC);

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

 {

};

