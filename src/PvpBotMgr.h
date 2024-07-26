
#ifndef _PVPBOTS_MGR_H
#define _PVPBOTS_MGR_H

#include "Common.h"

#include <vector>

class PvpBotMgr
{
    public:
        static PvpBotMgr* instance()
        {
            static PvpBotMgr instance;
            return &instance;
        }

        bool Initialize();

        std::vector<uint32> pvpBotAccounts;

};


#define sPvpMgr PvpBotMgr::instance()
#endif /* _PVPBOTS_MGR_H */
