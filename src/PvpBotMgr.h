
#ifndef _PVPBOTS_MGR_H
#define _PVPBOTS_MGR_H

#include "Common.h"
#include "PlayerbotMgr.h"
/*#include "QueryHolder.h"
#include "QueryResult.h"

#include <list>
#include <map>
#include <vector>*/

class CachedPvpEvent
{
    public:
        CachedPvpEvent() : value(0), lastChangeTime(0), validIn(0), data("") { }
        CachedPvpEvent(const CachedPvpEvent& other) : value(other.value), lastChangeTime(other.lastChangeTime), validIn(other.validIn), data(other.data) { }
        CachedPvpEvent(uint32 value, uint32 lastChangeTime, uint32 validIn, std::string const data = "") : value(value), lastChangeTime(lastChangeTime), validIn(validIn), data(data) { }

        bool IsEmpty() { return !lastChangeTime; }

    public:
        uint32 value;
        uint32 lastChangeTime;
        uint32 validIn;
        std::string data;
};

class PvpBotMgr : public PlayerbotHolder
{
    public:
        PvpBotMgr();
        virtual ~PvpBotMgr();
        static PvpBotMgr* instance()
        {
            static PvpBotMgr instance;
            return &instance;
        }

        //PlayerBotMap playerBots;

        void UpdateAIInternal(uint32 elapsed, bool minimal = false) override;
        uint32 AddPVPBots();

        uint32 GetEventValue(uint32 bot, std::string const event);
        uint32 SetEventValue(uint32 bot, std::string const event, uint32 value, uint32 validIn, std::string const data = "");

    protected:
        void OnBotLoginInternal(Player* const bot) override;

        bool ProcessBot(uint32 bot);
        bool ProcessBot(Player* player);

        void ScheduleRandomize(uint32 bot, uint32 time);
        void ScheduleTeleport(uint32 bot, uint32 time = 0);
        void ScheduleChangeStrategy(uint32 bot, uint32 time = 0);

        void Randomize(Player* bot);
        void Clear(Player* bot);
        void RandomizeFirst(Player* bot);
        void RandomizeMin(Player* bot);


        void Revive(Player* player);
        void Refresh(Player* bot);

        PlayerBotMap pvpBots;
        std::vector<uint32> pvpBotAccounts;
        std::list<uint32> currentBots;
        std::map<uint32, std::map<std::string, CachedPvpEvent>> eventCache;

};


#define sPvpMgr PvpBotMgr::instance()
#endif /* _PVPBOTS_MGR_H */
