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

#include "HonorMgr.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "World.h"

inline float finiteAlways(float f) { return std::isfinite(f) ? f : 0.0f; }

//TODOFROST - finish & check.

HonorMaintenancer* HonorMaintenancer::Instance()
{
    static HonorMaintenancer instance;
    return &instance;
}

void HonorMaintenancer::Initialize()
{
    TC_LOG_INFO("server.loading", "Initialize Honor Maintenance system...");

    m_lastMaintenanceDay = sWorld->GetPersistentWorldVariable(World::LastHonorMaintenanceTimeVarId);
    m_nextMaintenanceDay = sWorld->GetPersistentWorldVariable(World::NextHonorMaintenanceTimeVarId);

    const time_t current = std::chrono::duration_cast<Seconds>(GameTime::GetSystemTime().time_since_epoch()).count();
    if (!m_lastMaintenanceDay)
         SetMaintenanceDays(current);

    m_markerToStart = m_nextMaintenanceDay < current;
}

void HonorMaintenancer::DoMaintenance()
{
    if (!m_markerToStart)
        return;

    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Honor maintenance starting.");

    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Load weekly players scores.");
    LoadWeeklyScores();
    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Load standing lists.");
    LoadStandingLists();
    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Distribute rank points for Alliance.");
    DistributeRankPoints(TEAM_ALLIANCE);
    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Distribute rank points for Horde.");
    DistributeRankPoints(TEAM_HORDE);
    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Decay rank points for inactive players.");
    InactiveDecayRankPoints();

    //TODO - investigate if city protector is possible 
    // if (sWorld.getConfig(CONFIG_BOOL_ENABLE_CITY_PROTECTOR))
    // {
    //     sLog.Out(LOG_HONOR, LOG_LVL_BASIC, "[MAINTENANCE] Assign city titles.");
    //     SetCityRanks();
    // }

    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Flush rank points.");
    FlushRankPoints();

    // CreateCalculationReport();   //TODO
    
    //TODO should loaded data get unloaded?

    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Honor maintenance finished.");

    m_markerToStart = false;
    SetMaintenanceDays(m_nextMaintenanceDay);
}

void HonorMaintenancer::LoadWeeklyScores()
{
    const time_t weekBeginDay = m_lastMaintenanceDay;
    const time_t weekEndDay = m_lastMaintenanceDay + std::chrono::duration_cast<Seconds>(std::chrono::days(6)).count();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_HONOR_CP_WEEKLY_SCORES);
    stmt->setInt32(0, HONONR_TYPE_HONORABLE);
    stmt->setInt64(1, weekBeginDay);
    stmt->setInt64(2, weekEndDay);
    stmt->setInt32(3, HONONR_TYPE_DISHONORABLE);
    stmt->setInt64(4, weekBeginDay);
    stmt->setInt64(5, weekEndDay);
    stmt->setInt32(6, HONONR_TYPE_HONORABLE);
    stmt->setInt32(7, HONONR_TYPE_DISHONORABLE);
    stmt->setInt64(8, weekBeginDay);
    stmt->setInt64(9, weekEndDay);

    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            WeeklyScore score;
            score.level  = fields[1].GetUInt32();
            score.account = fields[2].GetUInt32();
            score.oldRp  = fields[3].GetFloat();
            score.highestRank = fields[4].GetUInt32();
            score.hk  = fields[5].GetUInt32();
            score.dk  = fields[6].GetUInt32();
            score.cp  = fields[7].GetFloat();
            m_weeklyScores[fields[0].GetUInt64()] = score;
        }
        while (result->NextRow());
    }
}

void HonorMaintenancer::LoadStandingLists()
{
    const int32 minHK = sWorld->getIntConfig(CONFIG_HONOR_MIN_KILLS);

    for (auto& pair : m_weeklyScores)
    {
        auto weeklyScore = pair.second;
        HonorStanding standing;
        standing.guid = pair.first;
        standing.cp = weeklyScore.cp;

        if (weeklyScore.hk < minHK || !weeklyScore.account)
        {
            m_inactiveStandingList.push_back(standing);
            continue;
        }

        const auto team = sCharacterCache->GetCharacterTeamByGuid(ObjectGuid::Create<HighGuid::Player>(pair.first));
        if (team == ALLIANCE)
            m_allianceStandingList.push_back(standing);
        else if (team == HORDE)
            m_hordeStandingList.push_back(standing);
    }

    // Make sure all things are sorted
    std::sort(m_allianceStandingList.begin(), m_allianceStandingList.end());
    std::sort(m_hordeStandingList.begin(), m_hordeStandingList.end());

    TC_LOG_INFO("server.loading", "[HONOR MAINTENANCE] Alliance: {}, Horde: {}, Inactive: {}", m_allianceStandingList.size(), m_hordeStandingList.size(), m_inactiveStandingList.size());
}

void HonorMaintenancer::DistributeRankPoints(TeamId team)
{
    HonorStandingList& standingList = team == TEAM_ALLIANCE ? m_allianceStandingList : m_hordeStandingList;
    if (standingList.empty())
        return;

    HonorScores scores = GenerateScores(standingList);
    uint32 position = 1;

    for (auto& standing : standingList)
    {
        auto itrWS = m_weeklyScores.find(standing.guid);
        if (itrWS == m_weeklyScores.end())
            continue;

        auto& weeklyScore = itrWS->second;

        // Calculate rank points earning
        weeklyScore.earning = CalculateRpEarning(weeklyScore.cp, scores);

        // Calculate rank points with decay
        weeklyScore.newRp = CalculateRpDecay(weeklyScore.earning, weeklyScore.oldRp);

        // Level restrictions
        weeklyScore.newRp = std::min(MaximumRpAtLevel(weeklyScore.level), weeklyScore.newRp);

        // Set standing to score
        weeklyScore.standing = position;

        ++position;
    }
}

void HonorMaintenancer::InactiveDecayRankPoints()
{
    for (auto& standing : m_inactiveStandingList)
    {
        auto itrWS = m_weeklyScores.find(standing.guid);
        if (itrWS == m_weeklyScores.end())
            continue;

        auto& weeklyScore = itrWS->second;
        weeklyScore.newRp =  finiteAlways(CalculateRpDecay(0, weeklyScore.oldRp));
    }
}

void HonorMaintenancer::FlushRankPoints()
{
    // Imediatly reset honor standing before flushing
    CharacterDatabase.Execute(CharacterDatabase.GetPreparedStatement(CHAR_UPD_RESET_HONOR_STANDING));

    for (auto& pair : m_weeklyScores)
    {
        auto weeklyScore = pair.second;

        //TODOFROST
        // HonorRankInfo currentRank = HonorMgr::CalculateRank(weeklyScore.newRp);
        // HonorRankInfo highestRank;
        // HonorMgr::InitRankInfo(highestRank);
        // highestRank.rank = weeklyScore.highestRank;
        // HonorMgr::CalculateRankInfo(highestRank);

        // if (currentRank.visualRank > 0 && (currentRank.visualRank > highestRank.visualRank))
        //     highestRank = currentRank;

        // CharacterDatabase.PExecute("UPDATE `characters` SET `honor_highest_rank` = %u, `honor_rank_points` = %.1f, `honor_standing` = %u, "
        //     "`honor_last_week_hk` = %u, `honor_stored_hk` = (`honor_stored_hk` + %u), `honor_stored_dk` = (`honor_stored_dk` + %u), `honor_last_week_cp` = %.1f WHERE `guid` = %u",
        //     highestRank.rank,
        //     finiteAlways(weeklyScore.newRp), weeklyScore.standing,
        //     weeklyScore.hk, weeklyScore.hk, weeklyScore.dk,
        //     finiteAlways(weeklyScore.cp), pair.first);
    }

    // Not includes weekend day, for correct view in honor tab for group "Yesterday"
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_HONOR_CP_ALL_PREVIOUS);
    stmt->setUInt64(0, m_lastMaintenanceDay + std::chrono::duration_cast<Seconds>(std::chrono::days(6)).count());
    CharacterDatabase.Execute(stmt);
}

float HonorMaintenancer::GetStandingCPByPosition(HonorStandingList const& standingList, uint32 position) const
{
    const uint32 index = position - 1;
    if (index < standingList.size())
        return standingList[index].cp;

    return 0.0f;
}

HonorScores HonorMaintenancer::GenerateScores(HonorStandingList const& standingList) const
{
    HonorScores sc;

    // (source: http://www.wowwiki.com/Honor_system_%28pre-2.0_formulas%29)
    // 1.12+
    sc.BRK[13] = 0.003f;
    sc.BRK[12] = 0.008f;
    sc.BRK[11] = 0.020f;
    sc.BRK[10] = 0.035f;
    sc.BRK[9] = 0.060f;
    sc.BRK[8] = 0.100f;
    sc.BRK[7] = 0.159f;
    sc.BRK[6] = 0.228f;
    sc.BRK[5] = 0.327f;
    sc.BRK[4] = 0.436f;
    sc.BRK[3] = 0.566f;
    sc.BRK[2] = 0.697f;
    sc.BRK[1] = 0.845f;
    sc.BRK[0] = 1.000f;
    
    const int32 configPoolSize = sWorld->getIntConfig(CONFIG_HONOR_POOL_SIZE_PER_FACTION);
    const size_t poolSize = configPoolSize == 0 ? standingList.size() : configPoolSize;
    // get the WS scores at the top of each break point
    for (float & group : sc.BRK)
        group = floor((group * poolSize) + 0.5f);

    // initialize RP array
    // set the low point
    sc.FY[0] = 0;

    // the Y values for each breakpoint are fixed
    sc.FY[1] = 400;
    for (uint8 i = 2; i <= 13; i++)
        sc.FY[i] = (i - 1) * 1000;

    // and finally
    sc.FY[14] = 13000;   // ... gets 13000 RP

    // the X values for each breakpoint are found from the CP scores
    // of the players around that point in the WS scores
    float honor;
    float tempHonor;

    // initialize CP array
    sc.FX[0] = 0;

    bool top = false;
    for (uint8 i = 1; i <= 13; i++)
    {
        honor = 0.0f;
        tempHonor = GetStandingCPByPosition(standingList, sc.BRK[i]);

        if (tempHonor)
        {
            honor += tempHonor;
            tempHonor = GetStandingCPByPosition(standingList, sc.BRK[i] + 1);

            if (tempHonor)
                honor += tempHonor;
        }

        sc.FX[i] = honor ? honor / 2 : 0;

        if (!top && !honor)
        {
            // top scorer
            sc.FX[i] = sc.FX[i - 1] ? standingList.begin()->cp : 0;
            top = true;
        }
    }

    // set the high point if FX full filled before
    // top scorer
    sc.FX[14] = !top ? standingList.begin()->cp : 0;

    return sc;
}

float HonorMaintenancer::CalculateRpEarning(float cp, HonorScores const& sc) const
{
    // search the function for the two points that bound the given CP
    uint8 i = 0;
    while (i < 14 && sc.BRK[i] > 0 && sc.FX[i] <= cp)
        i++;

    // we now have i such that FX[i] > cp >= FX[i-1]
    // so interpolate
    if (sc.FX[i] > cp && cp >= sc.FX[i - 1])
        return (sc.FY[i] - sc.FY[i - 1]) * (cp - sc.FX[i - 1]) / (sc.FX[i] - sc.FX[i - 1]) + sc.FY[i - 1];

    return sc.FY[i];
}

float HonorMaintenancer::CalculateRpDecay(float rpEarning, float rp) const
{
    const float decayMultiplier = sWorld->getFloatConfig(CONFIG_HONOR_RP_DECAY);
    const float decay = floor((decayMultiplier * rp) + 0.5f);
    float delta = rpEarning - decay;

    if (delta < 0)
        delta = delta / 2;

    if (delta < -2500)
        delta = -2500;

    return rp + delta;
}

constexpr float HonorMaintenancer::MaximumRpAtLevel(uint8 level) const
{
    if (level <= 29)
        return 6500;
    if (level >= 30 && level <= 35)
        return 7150 + 975 * (level - 30);
    if (level >= 36 && level <= 39)
        return 12025 + 1300 * (level - 35);
    if (level >= 40 && level <= 43)
        return 17225 + 1625 * (level - 39);
    if (level >= 44 && level <= 52)
        return 23725 + 2275 * (level - 43);
    if (level >= 53 && level <= 60)
        return 44200 + 2600 * (level - 52);
    return 65000;
}

void HonorMaintenancer::SetMaintenanceDays(time_t last, time_t next)
{
    m_lastMaintenanceDay = last;

    if (!next)
        m_nextMaintenanceDay = m_lastMaintenanceDay + std::chrono::duration_cast<Seconds>(std::chrono::days(7)).count();

    //TODOFROST - should be rounding to start of day?

    sWorld->SetPersistentWorldVariable(World::LastHonorMaintenanceTimeVarId, m_lastMaintenanceDay);
    sWorld->SetPersistentWorldVariable(World::NextHonorMaintenanceTimeVarId, m_nextMaintenanceDay);
}
