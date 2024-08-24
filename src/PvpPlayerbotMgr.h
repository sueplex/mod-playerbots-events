/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_PVPPLAYERBOTMGR_H
#define _PLAYERBOT_PVPPLAYERBOTMGR_H

#include "Common.h"
#include "Player.h"
#include "PlayerbotAIBase.h"
#include "QueryHolder.h"
#include "QueryResult.h"

/*class ChatHandler;
class PlayerbotAI;
class PlayerbotLoginQueryHolder;
class WorldPacket;

typedef std::map<ObjectGuid, Player*> PlayerBotMap;
typedef std::map<std::string, std::set<std::string> > PlayerBotErrorMap;*/

class PvpPlayerbotHolder : public PlayerbotAIBase
{
public:
    PvpPlayerbotHolder();
    virtual ~PvpPlayerbotHolder(){};

    void AddPlayerBot(ObjectGuid guid, uint32 masterAccountId);
    void HandlePlayerBotLoginCallback(PlayerbotLoginQueryHolder const& holder);

    void LogoutPlayerBot(ObjectGuid guid);
    void DisablePlayerBot(ObjectGuid guid);
    Player* GetPlayerBot(ObjectGuid guid) const;
    Player* GetPlayerBot(ObjectGuid::LowType lowGuid) const;
    PlayerBotMap::const_iterator GetPlayerBotsBegin() const { return playerBots.begin(); }
    PlayerBotMap::const_iterator GetPlayerBotsEnd() const { return playerBots.end(); }

    void UpdateAIInternal([[maybe_unused]] uint32 elapsed, [[maybe_unused]] bool minimal = false) override{};
    void UpdateSessions();
    void HandleBotPackets(WorldSession* session);

    void LogoutAllBots();
    void OnBotLogin(Player* const bot);

    std::vector<std::string> HandlePvpPlayerbotCommand(char const* args, Player* master = nullptr);
    std::string const ProcessBotCommand(std::string const cmd, ObjectGuid guid, ObjectGuid masterguid, bool admin,
                                        uint32 masterAccountId, uint32 masterGuildId);
    uint32 GetAccountId(std::string const name);
    uint32 GetAccountId(ObjectGuid guid);
    std::string const ListBots(Player* master);
    std::string const LookupBots(Player* master);
    uint32 GetPvpPlayerbotsCount() { return playerBots.size(); }
    uint32 GetPvpPlayerbotsCountByClass(uint32 cls);

protected:
    virtual void OnBotLoginInternal(Player* const bot) = 0;

    PlayerBotMap playerBots;
};

class PvpPlayerbotMgr : public PvpPlayerbotHolder
{
public:
    PvpPlayerbotMgr(Player* const master);
    virtual ~PvpPlayerbotMgr();

    static bool HandlePvpPlayerbotMgrCommand(ChatHandler* handler, char const* args);
    void HandleMasterIncomingPacket(WorldPacket const& packet);
    void HandleMasterOutgoingPacket(WorldPacket const& packet);
    void HandleCommand(uint32 type, std::string const text);
    void OnPlayerLogin(Player* player);
    void CancelLogout();

    void UpdateAIInternal(uint32 elapsed, bool minimal = false) override;
    void TellError(std::string const botName, std::string const text);

    Player* GetMaster() const { return master; };

    void SaveToDB();

protected:
    void OnBotLoginInternal(Player* const bot) override;
    void CheckTellErrors(uint32 elapsed);

private:
    Player* const master;
    PlayerBotErrorMap errors;
    time_t lastErrorTell;
};

class PvpPlayerbotsMgr
{
public:
    PvpPlayerbotsMgr() {}
    ~PvpPlayerbotsMgr() {}

    static PvpPlayerbotsMgr* instance()
    {
        static PvpPlayerbotsMgr instance;
        return &instance;
    }

    void AddPvpPlayerbotData(Player* player, bool isBotAI);
    void RemovePvpPlayerBotData(ObjectGuid const& guid, bool is_AI);

    PlayerbotAI* GetPvpPlayerbotAI(Player* player);
    PvpPlayerbotMgr* GetPvpPlayerbotMgr(Player* player);

private:
    std::unordered_map<ObjectGuid, PlayerbotAIBase*> _playerbotsAIMap;
    std::unordered_map<ObjectGuid, PlayerbotAIBase*> _playerbotsMgrMap;
};

#define sPvpPlayerbotsMgr PvpPlayerbotsMgr::instance()

#endif
