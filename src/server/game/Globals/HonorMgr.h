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

#ifndef HONORMGR_H
#define HONORMGR_H

#include "Common.h"
#include "ObjectGuid.h"

struct HonorScores
{
    std::array<float, 15> FX;
    std::array<float, 15> FY;
    std::array<float, 14> BRK;
};

struct HonorStanding
{
    HonorStanding() {}

    ObjectGuid::LowType guid = 0;
    float cp = 0.0f;

    // create the standing order
    bool operator < (HonorStanding const& hs) const
    {
        return cp > hs.cp;
    }
};

typedef std::vector<HonorStanding> HonorStandingList;

enum HonorType : uint8
{
    HONONR_TYPE_HONORABLE    = 1,
    HONONR_TYPE_DISHONORABLE = 2,
    HONONR_TYPE_BONUS        = 3,
    HONONR_TYPE_QUEST        = 4,
    HONONR_TYPE_OTHER        = 5
};

enum HonorState : uint8
{
   HONOR_STATE_NEW       = 0,
   HONOR_STATE_UNCHANGED = 1
};

struct HonorCP
{
   uint8  victimType;
   ObjectGuid::LowType victimId;
   float  cp;
   time_t date;
   HonorType type;
   HonorState state;
};

struct HonorRankInfo
{
    HonorRankInfo() {}

    uint8 rank = 0;       // internal range [0..18]
    int8 visualRank = 0;  // number visualized in rank bar [-4..14]
    float maxRP = 0.0f;
    float minRP = 0.0f;
    bool positive = true;
};

struct WeeklyScore
{
    WeeklyScore() {}

    uint8  level = 0;
    uint32 account = 0;
    uint32 hk = 0;
    uint32 dk = 0;
    float  cp = 0.0f;
    float  oldRp = 0.0f;
    float  newRp = 0.0f;
    float  earning = 0.0f;
    uint32 standing = 0;
    uint8  highestRank = 0;
};

class TC_GAME_API HonorMaintenancer
{
public:
    HonorMaintenancer() {}
    static HonorMaintenancer* Instance();

    void Initialize();
    void DoMaintenance();

private:
    void LoadWeeklyScores();
    void LoadStandingLists();
    void DistributeRankPoints(TeamId team);
    void InactiveDecayRankPoints();
    void FlushRankPoints();

    float GetStandingCPByPosition(HonorStandingList const& standingList, uint32 position) const;
    HonorScores GenerateScores(HonorStandingList const& standingList) const;
    float CalculateRpEarning(float cp, HonorScores const& sc) const;
    float CalculateRpDecay(float rpEarning, float rp) const;
    constexpr float MaximumRpAtLevel(uint8 level) const;
    void SetMaintenanceDays(time_t last, time_t next = 0);

    HonorStandingList m_hordeStandingList;
    HonorStandingList m_allianceStandingList;
    HonorStandingList m_inactiveStandingList;
    std::unordered_map<ObjectGuid::LowType, WeeklyScore> m_weeklyScores;

    time_t m_lastMaintenanceDay = 0;
    time_t m_nextMaintenanceDay = 0;
    bool m_markerToStart = false;
};

#define sHonorMaintenancer HonorMaintenancer::Instance()

#endif
