/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
SDName: Instance_Wailing_Caverns
SD%Complete: 99
SDComment: Everything seems to work, still need some checking
SDCategory: Wailing Caverns
EndScriptData */

#include "ScriptMgr.h"
#include "Creature.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "wailing_caverns.h"
#include <sstream>

#define MAX_ENCOUNTER   9

class instance_wailing_caverns : public InstanceMapScript
{
public:
    instance_wailing_caverns() : InstanceMapScript(WCScriptName, 43) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_wailing_caverns_InstanceMapScript(map);
    }

    struct instance_wailing_caverns_InstanceMapScript : public InstanceScript
    {
        instance_wailing_caverns_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

            yelled = false;
        }

        uint32 m_auiEncounter[MAX_ENCOUNTER];

        bool yelled;
        ObjectGuid NaralexGUID;

        void OnCreatureCreate(Creature* creature) override
        {
            if (creature->GetEntry() == DATA_NARALEX)
                NaralexGUID = creature->GetGUID();
        }

        void OnUnitDeath(Unit* unit) override
        {
            switch (unit->GetEntry())
            {
            case NPC_KRESH:                 SetBossState(BOSS_KRESH, DONE); break;
            case NPC_SKUM:                  SetBossState(BOSS_SKUM, DONE); break;
            case NPC_VERDAN_THE_EVERLIVING: SetBossState(BOSS_VERDAN_THE_EVERLIVING, DONE); break;
            default:
                break;
            }
        }

        void SetData(uint32 type, uint32 data) override
        {
            switch (type)
            {
                case TYPE_LORD_COBRAHN:         m_auiEncounter[0] = data;break;
                case TYPE_LORD_PYTHAS:          m_auiEncounter[1] = data;break;
                case TYPE_LADY_ANACONDRA:       m_auiEncounter[2] = data;break;
                case TYPE_LORD_SERPENTIS:       m_auiEncounter[3] = data;break;
                case TYPE_NARALEX_EVENT:        m_auiEncounter[4] = data;break;
                case TYPE_NARALEX_PART1:        m_auiEncounter[5] = data;break;
                case TYPE_NARALEX_PART2:        m_auiEncounter[6] = data;break;
                case TYPE_NARALEX_PART3:        m_auiEncounter[7] = data;break;
                case TYPE_MUTANUS_THE_DEVOURER: m_auiEncounter[8] = data;break;
                case TYPE_NARALEX_YELLED:       yelled = true;      break;
            }
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
                case TYPE_LORD_COBRAHN:         return m_auiEncounter[0];
                case TYPE_LORD_PYTHAS:          return m_auiEncounter[1];
                case TYPE_LADY_ANACONDRA:       return m_auiEncounter[2];
                case TYPE_LORD_SERPENTIS:       return m_auiEncounter[3];
                case TYPE_NARALEX_EVENT:        return m_auiEncounter[4];
                case TYPE_NARALEX_PART1:        return m_auiEncounter[5];
                case TYPE_NARALEX_PART2:        return m_auiEncounter[6];
                case TYPE_NARALEX_PART3:        return m_auiEncounter[7];
                case TYPE_MUTANUS_THE_DEVOURER: return m_auiEncounter[8];
                case TYPE_NARALEX_YELLED:       return yelled;
            }
            return 0;
        }

        ObjectGuid GetGuidData(uint32 data) const override
        {
            if (data == DATA_NARALEX)return NaralexGUID;
            return ObjectGuid::Empty;
        }

    };

};

void AddSC_instance_wailing_caverns()
{
    new instance_wailing_caverns();
}
