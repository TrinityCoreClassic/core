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

#include "Loot.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Group.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "LootMgr.h"
#include "LootPackets.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "World.h"
#include "WorldSession.h"

//
// --------- LootItem ---------
//

// Constructor, copies most fields from LootStoreItem and generates random count
LootItem::LootItem(LootStoreItem const& li)
{
    itemid = li.itemid;
    itemIndex = 0;
    conditions = li.conditions;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemid);
    freeforall = proto && proto->HasFlag(ITEM_FLAG_MULTI_DROP);
    follow_loot_rules = proto && (proto->HasFlag(ITEM_FLAGS_CU_FOLLOW_LOOT_RULES));

    needs_quest = li.needs_quest;

    randomSuffix = GenerateEnchSuffixFactor(itemid);
    randomPropertyId = GenerateItemRandomPropertyId(itemid);
    context = ItemContext::NONE;
    count = 0;
    is_looted = 0;
    is_blocked = 0;
    is_underthreshold = 0;
    is_counted = 0;
    rollWinnerGUID = ObjectGuid::Empty;
}

LootItem::LootItem(LootItem const&) = default;
LootItem::LootItem(LootItem&&) noexcept = default;
LootItem& LootItem::operator=(LootItem const&) = default;
LootItem& LootItem::operator=(LootItem&&) noexcept = default;
LootItem::~LootItem() = default;

// Basic checks for player/item compatibility - if false no chance to see the item in the loot
bool LootItem::AllowedForPlayer(Player const* player, bool isGivenByMasterLooter, ObjectGuid ownerGuid) const
{
    // DB conditions check
    if (!sConditionMgr->IsObjectMeetToConditions(const_cast<Player*>(player), conditions))
        return false;

    ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(itemid);
    if (!pProto)
        return false;

    // not show loot for not own team
    if (pProto->HasFlag(ITEM_FLAG2_FACTION_HORDE) && player->GetTeam() != HORDE)
        return false;

    if (pProto->HasFlag(ITEM_FLAG2_FACTION_ALLIANCE) && player->GetTeam() != ALLIANCE)
        return false;

    // Master looter can see all items even if the character can't loot them
    if (!isGivenByMasterLooter && player->GetGroup() && player->GetGroup()->GetMasterLooterGuid() == player->GetGUID())
    {
        return true;
    }

    // Don't allow loot for players without profession or those who already know the recipe
    if (pProto->HasFlag(ITEM_FLAG_HIDE_UNUSABLE_RECIPE))
    {
        if (!player->HasSkill(pProto->GetRequiredSkill()))
            return false;

        for (ItemEffectEntry const* itemEffect : pProto->Effects)
        {
            if (itemEffect->TriggerType != ITEM_SPELLTRIGGER_ON_LEARN)
                continue;

            if (player->HasSpell(itemEffect->SpellID))
                return false;
        }
    }

    // Don't allow to loot soulbound recipes that the player has already learned
    if (pProto->GetClass() == ITEM_CLASS_RECIPE && pProto->GetBonding() == BIND_ON_ACQUIRE)
    {
        for (ItemEffectEntry const* itemEffect : pProto->Effects)
        {
            if (itemEffect->TriggerType != ITEM_SPELLTRIGGER_ON_LEARN)
                continue;

            if (player->HasSpell(itemEffect->SpellID))
                return false;
        }
    }

    if (needs_quest && !freeforall && player->GetGroup() && (player->GetGroup()->GetLootMethod() == GROUP_LOOT || player->GetGroup()->GetLootMethod() == ROUND_ROBIN) && !ownerGuid.IsEmpty() && ownerGuid != player->GetGUID())
        return false;

    // check quest requirements
    if (!pProto->HasFlag(ITEM_FLAGS_CU_IGNORE_QUEST_STATUS) && ((needs_quest || (pProto->GetStartQuest() && player->GetQuestStatus(pProto->GetStartQuest()) != QUEST_STATUS_NONE)) && !player->HasQuestForItem(itemid)))
        return false;

    return true;
}

void LootItem::AddAllowedLooter(const Player* player)
{
    allowedGUIDs.insert(player->GetGUID());
}

bool LootItem::HasAllowedLooter(ObjectGuid const& looter) const
{
    return allowedGUIDs.contains(looter);
}

//
// ------- Loot Roll -------
//

// Send the roll for the whole group
void LootRoll::SendStartRoll()
{
    ItemTemplate const* itemTemplate = ASSERT_NOTNULL(sObjectMgr->GetItemTemplate(m_lootItem->itemid));
    //for (auto const& [playerGuid, roll] : m_rollVoteMap)
    //{
    //    if (roll.Vote != RollVote::NotEmitedYet)
    //        continue;

    //    Player* player = ObjectAccessor::GetPlayer(m_map, playerGuid);
    //    if (!player)
    //        continue;

    //    WorldPackets::Loot::StartLootRoll startLootRoll;
    //    startLootRoll.LootObj = m_loot->GetGUID();
    //    startLootRoll.MapID = m_map->GetId();
    //    startLootRoll.RollTime = LOOT_ROLL_TIMEOUT;
    //    startLootRoll.Method = m_loot->GetLootMethod();
    //    startLootRoll.ValidRolls = m_voteMask;
    //    // In NEED_BEFORE_GREED need disabled for non-usable item for player
    //    if (m_loot->GetLootMethod() == NEED_BEFORE_GREED && player->CanRollNeedForItem(itemTemplate, m_map, true) != EQUIP_ERR_OK)
    //        startLootRoll.ValidRolls &= ~ROLL_FLAG_TYPE_NEED;

    //    FillPacket(startLootRoll.Item);
    //    startLootRoll.Item.UIType = LOOT_SLOT_TYPE_ROLL_ONGOING;
    //    startLootRoll.DungeonEncounterID = m_loot->GetDungeonEncounterId();

    //    player->SendDirectMessage(startLootRoll.Write());
    //}

    // Handle auto pass option
    for (auto const& [playerGuid, roll] : m_rollVoteMap)
    {
        if (roll.Vote != RollVote::Pass)
            continue;

        SendRoll(playerGuid, -1, RollVote::Pass, {});
    }
}

// Send all passed message
void LootRoll::SendAllPassed()
{
    WorldPackets::Loot::LootAllPassed lootAllPassed;
    lootAllPassed.LootObj = m_loot->GetGUID();
    FillPacket(lootAllPassed.Item);
    lootAllPassed.Item.UIType = LOOT_SLOT_TYPE_ALLOW_LOOT;
    //lootAllPassed.DungeonEncounterID = m_loot->GetDungeonEncounterId();
    lootAllPassed.Write();

    for (auto const& [playerGuid, roll] : m_rollVoteMap)
    {
        if (roll.Vote != RollVote::NotValid)
            continue;

        Player* player = ObjectAccessor::GetPlayer(m_map, playerGuid);
        if (!player)
            continue;

        player->SendDirectMessage(lootAllPassed.GetRawPacket());
    }
}

// Send roll of targetGuid to the whole group (included targuetGuid)
void LootRoll::SendRoll(ObjectGuid const& targetGuid, int32 rollNumber, RollVote rollType, Optional<ObjectGuid> const& rollWinner)
{
    WorldPackets::Loot::LootRollBroadcast lootRoll;
    lootRoll.LootObj = m_loot->GetGUID();
    lootRoll.Player = targetGuid;
    lootRoll.Roll = rollNumber;
    lootRoll.RollType = AsUnderlyingType(rollType);
    lootRoll.Autopassed = false;
    FillPacket(lootRoll.Item);
    lootRoll.Item.UIType = LOOT_SLOT_TYPE_ROLL_ONGOING;
    //lootRoll.DungeonEncounterID = m_loot->GetDungeonEncounterId();
    lootRoll.Write();

    for (auto const& [playerGuid, roll] : m_rollVoteMap)
    {
        if (roll.Vote == RollVote::NotValid)
            continue;

        if (playerGuid == rollWinner)
            continue;

        Player* player = ObjectAccessor::GetPlayer(m_map, playerGuid);
        if (!player)
            continue;

        player->SendDirectMessage(lootRoll.GetRawPacket());
    }

    if (rollWinner)
    {
        if (Player* player = ObjectAccessor::GetPlayer(m_map, *rollWinner))
        {
            lootRoll.Item.UIType = LOOT_SLOT_TYPE_ALLOW_LOOT;
            lootRoll.Clear();
            player->SendDirectMessage(lootRoll.Write());
        }
    }
}

// Send roll 'value' of the whole group and the winner to the whole group
void LootRoll::SendLootRollWon(ObjectGuid const& targetGuid, int32 rollNumber, RollVote rollType)
{
    // Send roll values
    for (auto const& [playerGuid, roll] : m_rollVoteMap)
    {
        switch (roll.Vote)
        {
        case RollVote::Pass:
            break;
        case RollVote::NotEmitedYet:
        case RollVote::NotValid:
            SendRoll(playerGuid, 0, RollVote::Pass, targetGuid);
            break;
        default:
            SendRoll(playerGuid, roll.RollNumber, roll.Vote, targetGuid);
            break;
        }
    }

    WorldPackets::Loot::LootRollWon lootRollWon;
    lootRollWon.LootObj = m_loot->GetGUID();
    lootRollWon.Winner = targetGuid;
    lootRollWon.Roll = rollNumber;
    lootRollWon.RollType = AsUnderlyingType(rollType);
    FillPacket(lootRollWon.Item);
    lootRollWon.Item.UIType = LOOT_SLOT_TYPE_LOCKED;
    //lootRollWon.DungeonEncounterID = m_loot->GetDungeonEncounterId();
    lootRollWon.MainSpec = true;    // offspec rolls not implemented
    lootRollWon.Write();

    for (auto const& [playerGuid, roll] : m_rollVoteMap)
    {
        if (roll.Vote == RollVote::NotValid)
            continue;

        if (playerGuid == targetGuid)
            continue;

        Player* player = ObjectAccessor::GetPlayer(m_map, playerGuid);
        if (!player)
            continue;

        player->SendDirectMessage(lootRollWon.GetRawPacket());
    }

    if (Player* player = ObjectAccessor::GetPlayer(m_map, targetGuid))
    {
        lootRollWon.Item.UIType = LOOT_SLOT_TYPE_ALLOW_LOOT;
        lootRollWon.Clear();
        player->SendDirectMessage(lootRollWon.Write());
    }
}

void LootRoll::FillPacket(WorldPackets::Loot::LootItemData& lootItem) const
{
    lootItem.Quantity = m_lootItem->count;
    //lootItem.LootListID = m_lootItem->LootListId;
    lootItem.CanTradeToTapList = m_lootItem->allowedGUIDs.size() > 1;
    lootItem.Loot.Initialize(*m_lootItem);
}

LootRoll::~LootRoll()
{
    if (m_isStarted)
        SendAllPassed();

    for (auto const& [playerGuid, roll] : m_rollVoteMap)
    {
        if (roll.Vote != RollVote::NotEmitedYet)
            continue;

        Player* player = ObjectAccessor::GetPlayer(m_map, playerGuid);
        if (!player)
            continue;

        player->RemoveLootRoll(this);
    }
}

// Try to start the group roll for the specified item (it may fail for quest item or any condition
// If this method return false the roll have to be removed from the container to avoid any problem
bool LootRoll::TryToStart(Map* map, Loot& loot, uint32 lootListId, uint16 enchantingSkill)
{
    if (!m_isStarted)
    {
        if (lootListId >= loot.items.size())
            return false;

        m_map = map;

        // initialize the data needed for the roll
        m_lootItem = &loot.items[lootListId];

        m_loot = &loot;
        m_lootItem->is_blocked = true;                          // block the item while rolling

        uint32 playerCount = 0;
        for (ObjectGuid allowedLooter : m_lootItem->GetAllowedLooters())
        {
            Player* plr = ObjectAccessor::GetPlayer(m_map, allowedLooter);
            if (!plr || !m_lootItem->HasAllowedLooter(plr->GetGUID()))     // check if player meet the condition to be able to roll this item
            {
                m_rollVoteMap[allowedLooter].Vote = RollVote::NotValid;
                continue;
            }
            // initialize player vote map
            m_rollVoteMap[allowedLooter].Vote = plr->GetPassOnGroupLoot() ? RollVote::Pass : RollVote::NotEmitedYet;
            if (!plr->GetPassOnGroupLoot())
                plr->AddLootRoll(this);

            ++playerCount;
        }

        // initialize item prototype and check enchant possibilities for this group
        ItemTemplate const* itemTemplate = ASSERT_NOTNULL(sObjectMgr->GetItemTemplate(m_lootItem->itemid));
        m_voteMask = ROLL_ALL_TYPE_MASK;
        if (itemTemplate->HasFlag(ITEM_FLAG2_CAN_ONLY_ROLL_GREED))
            m_voteMask = RollMask(m_voteMask & ~ROLL_FLAG_TYPE_NEED);
        if (Optional<uint16> disenchantSkillRequired = GetItemDisenchantSkillRequired(); !disenchantSkillRequired || disenchantSkillRequired > enchantingSkill)
            m_voteMask = RollMask(m_voteMask & ~ROLL_FLAG_TYPE_DISENCHANT);

        if (playerCount > 1)                                    // check if more than one player can loot this item
        {
            // start the roll
            SendStartRoll();
            m_endTime = GameTime::Now() + LOOT_ROLL_TIMEOUT;
            m_isStarted = true;
            return true;
        }
        // no need to start roll if one or less player can loot this item so place it under threshold
        m_lootItem->is_underthreshold = true;
        m_lootItem->is_blocked = false;
    }
    return false;
}

// Add vote from playerGuid
bool LootRoll::PlayerVote(Player* player, RollVote vote)
{
    ObjectGuid const& playerGuid = player->GetGUID();
    RollVoteMap::iterator voterItr = m_rollVoteMap.find(playerGuid);
    if (voterItr == m_rollVoteMap.end())
        return false;

    voterItr->second.Vote = vote;

    if (vote != RollVote::Pass && vote != RollVote::NotValid)
        voterItr->second.RollNumber = urand(1, 100);

    switch (vote)
    {
    case RollVote::Pass:                                // Player choose pass
    {
        SendRoll(playerGuid, -1, RollVote::Pass, {});
        break;
    }
    case RollVote::Need:                                // player choose Need
    {
        SendRoll(playerGuid, 0, RollVote::Need, {});
        player->UpdateCriteria(CriteriaType::RollAnyNeed, 1);
        break;
    }
    case RollVote::Greed:                               // player choose Greed
    {
        SendRoll(playerGuid, -1, RollVote::Greed, {});
        player->UpdateCriteria(CriteriaType::RollAnyGreed, 1);
        break;
    }
    case RollVote::Disenchant:                          // player choose Disenchant
    {
        SendRoll(playerGuid, -1, RollVote::Disenchant, {});
        player->UpdateCriteria(CriteriaType::RollAnyGreed, 1);
        break;
    }
    default:                                            // Roll removed case
        return false;
    }
    return true;
}

// check if we can found a winner for this roll or if timer is expired
bool LootRoll::UpdateRoll()
{
    RollVoteMap::const_iterator winnerItr = m_rollVoteMap.end();

    if (AllPlayerVoted(winnerItr) || m_endTime <= GameTime::Now())
    {
        Finish(winnerItr);
        return true;
    }
    return false;
}

bool LootRoll::IsLootItem(ObjectGuid const& lootObject, uint32 lootListId) const
{
    return m_loot->GetGUID() == lootObject; //&& m_lootItem->LootListId == lootListId;
}

/**
* \brief Check if all player have voted and return true in that case. Also return current winner.
* \param winnerItr > will be different than m_rollCoteMap.end() if winner exist. (Someone voted greed or need)
* \returns true if all players voted
**/
bool LootRoll::AllPlayerVoted(RollVoteMap::const_iterator& winnerItr)
{
    uint32 notVoted = 0;
    bool isSomeoneNeed = false;

    winnerItr = m_rollVoteMap.end();
    for (RollVoteMap::const_iterator itr = m_rollVoteMap.begin(); itr != m_rollVoteMap.end(); ++itr)
    {
        switch (itr->second.Vote)
        {
        case RollVote::Need:
            if (!isSomeoneNeed || winnerItr == m_rollVoteMap.end() || itr->second.RollNumber > winnerItr->second.RollNumber)
            {
                isSomeoneNeed = true;                                               // first passage will force to set winner because need is prioritized
                winnerItr = itr;
            }
            break;
        case RollVote::Greed:
        case RollVote::Disenchant:
            if (!isSomeoneNeed)                                                      // if at least one need is detected then winner can't be a greed
            {
                if (winnerItr == m_rollVoteMap.end() || itr->second.RollNumber > winnerItr->second.RollNumber)
                    winnerItr = itr;
            }
            break;
            // Explicitly passing excludes a player from winning loot, so no action required.
        case RollVote::Pass:
            break;
        case RollVote::NotEmitedYet:
            ++notVoted;
            break;
        default:
            break;
        }
    }

    return notVoted == 0;
}

Optional<uint32> LootRoll::GetItemDisenchantLootId() const
{
    WorldPackets::Item::ItemInstance itemInstance;
    itemInstance.Initialize(*m_lootItem);

    BonusData bonusData;
    bonusData.Initialize(itemInstance);
    if (!bonusData.CanDisenchant)
        return {};

    if (bonusData.DisenchantLootId)
        return bonusData.DisenchantLootId;

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(m_lootItem->itemid);

    //TODOFROST
    //// ignore temporary item level scaling (pvp or timewalking)
    //uint32 itemLevel = Item::GetItemLevel(itemTemplate, bonusData, bonusData.RequiredLevel, 0, 0, 0, 0, false, 0);

    //ItemDisenchantLootEntry const* disenchantLoot = Item::GetBaseDisenchantLoot(itemTemplate, bonusData.Quality, itemLevel);
    //if (!disenchantLoot)
    //    return {};

    //return disenchantLoot->ID;

    return {};
}

Optional<uint16> LootRoll::GetItemDisenchantSkillRequired() const
{
    WorldPackets::Item::ItemInstance itemInstance;
    itemInstance.Initialize(*m_lootItem);

    BonusData bonusData;
    bonusData.Initialize(itemInstance);
    if (!bonusData.CanDisenchant)
        return {};

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(m_lootItem->itemid);

    //TODOFROST
    //// ignore temporary item level scaling (pvp or timewalking)
    //uint32 itemLevel = Item::GetItemLevel(itemTemplate, bonusData, bonusData.RequiredLevel, 0, 0, 0, 0, false, 0);

    //ItemDisenchantLootEntry const* disenchantLoot = Item::GetBaseDisenchantLoot(itemTemplate, bonusData.Quality, itemLevel);
    //if (!disenchantLoot)
    //    return {};

    //return disenchantLoot->SkillRequired;

    return {};
}

// terminate the roll
void LootRoll::Finish(RollVoteMap::const_iterator winnerItr)
{
    m_lootItem->is_blocked = false;
    if (winnerItr == m_rollVoteMap.end())
    {
        SendAllPassed();
    }
    else
    {
        m_lootItem->rollWinnerGUID = winnerItr->first;

        SendLootRollWon(winnerItr->first, winnerItr->second.RollNumber, winnerItr->second.Vote);

        if (Player* player = ObjectAccessor::FindConnectedPlayer(winnerItr->first))
        {
            if (winnerItr->second.Vote == RollVote::Need)
                player->UpdateCriteria(CriteriaType::RollNeed, m_lootItem->itemid, winnerItr->second.RollNumber);
            else if (winnerItr->second.Vote == RollVote::Disenchant)
                player->UpdateCriteria(CriteriaType::CastSpell, 13262);
            else
                player->UpdateCriteria(CriteriaType::RollGreed, m_lootItem->itemid, winnerItr->second.RollNumber);

            //TODOFROST
            //if (winnerItr->second.Vote == RollVote::Disenchant)
            //{
            //    Loot loot(m_map, m_loot->GetOwnerGUID(), LOOT_DISENCHANTING, nullptr);
            //    loot.FillLoot(*GetItemDisenchantLootId(), LootTemplates_Disenchant, player, true, false, LOOT_MODE_DEFAULT, ItemContext::NONE);
            //    if (!loot.AutoStore(player, NULL_BAG, NULL_SLOT, true))
            //    {
            //        for (uint32 i = 0; i < loot.items.size(); ++i)
            //            if (LootItem* disenchantLoot = loot.LootItemInSlot(i, player))
            //                if (disenchantLoot->type == LootItemType::Item)
            //                    player->SendItemRetrievalMail(disenchantLoot->itemid, disenchantLoot->count, disenchantLoot->context);
            //    }
            //    else
            //        m_loot->NotifyItemRemoved(m_lootItem->LootListId, m_map);
            //}
            //else
            //    player->StoreLootItem(m_loot->GetOwnerGUID(), m_lootItem->LootListId, m_loot);
        }
    }
    m_isStarted = false;
}

//
// --------- Loot ---------
//

Loot::Loot(Map* map, ObjectGuid owner, LootType type) : gold(0), unlootedCount(0), roundRobinPlayer(), loot_type(type), maxDuplicates(1),
    _guid(map ? ObjectGuid::Create<HighGuid::LootObject>(map->GetId(), 0, map->GenerateLowGuid<HighGuid::LootObject>()) : ObjectGuid::Empty),
    _owner(owner), _itemContext(ItemContext::NONE)
{
}

Loot::~Loot()
{
    clear();
}

void Loot::clear()
{
    for (NotNormalLootItemMap::const_iterator itr = PlayerQuestItems.begin(); itr != PlayerQuestItems.end(); ++itr)
        delete itr->second;
    PlayerQuestItems.clear();

    for (NotNormalLootItemMap::const_iterator itr = PlayerFFAItems.begin(); itr != PlayerFFAItems.end(); ++itr)
        delete itr->second;
    PlayerFFAItems.clear();

    for (NotNormalLootItemMap::const_iterator itr = PlayerNonQuestNonFFAConditionalItems.begin(); itr != PlayerNonQuestNonFFAConditionalItems.end(); ++itr)
        delete itr->second;
    PlayerNonQuestNonFFAConditionalItems.clear();

    PlayersLooting.clear();
    items.clear();
    quest_items.clear();
    gold = 0;
    unlootedCount = 0;
    roundRobinPlayer.Clear();
    loot_type = LOOT_NONE;
    _itemContext = ItemContext::NONE;
}

void Loot::NotifyItemRemoved(uint8 lootIndex, Map const* map)
{
    // notify all players that are looting this that the item was removed
    // convert the index to the slot the player sees
    for (auto itr = PlayersLooting.begin(); itr != PlayersLooting.end();)
    {
        if (Player* player = ObjectAccessor::GetPlayer(map, *itr))
        {
            player->SendNotifyLootItemRemoved(GetGUID(), GetOwnerGUID(), lootIndex);
            ++itr;
        }
        else
            itr = PlayersLooting.erase(itr);
    }
}

void Loot::NotifyQuestItemRemoved(uint8 questIndex, Map const* map)
{
    // when a free for all questitem is looted
    // all players will get notified of it being removed
    // (other questitems can be looted by each group member)
    // bit inefficient but isn't called often
    for (auto itr = PlayersLooting.begin(); itr != PlayersLooting.end();)
    {
        if (Player* player = ObjectAccessor::GetPlayer(map, *itr))
        {
            NotNormalLootItemMap::const_iterator pq = PlayerQuestItems.find(player->GetGUID());
            if (pq != PlayerQuestItems.end() && pq->second)
            {
                // find where/if the player has the given item in it's vector
                NotNormalLootItemList& pql = *pq->second;

                uint8 j;
                for (j = 0; j < pql.size(); ++j)
                    if (pql[j].index == questIndex)
                        break;

                if (j < pql.size())
                    player->SendNotifyLootItemRemoved(GetGUID(), GetOwnerGUID(), items.size() + j);
            }

            ++itr;
        }
        else
            itr = PlayersLooting.erase(itr);
    }
}

void Loot::NotifyMoneyRemoved(Map const* map)
{
    // notify all players that are looting this that the money was removed
    for (auto itr = PlayersLooting.begin(); itr != PlayersLooting.end();)
    {
        if (Player* player = ObjectAccessor::GetPlayer(map, *itr))
        {
            player->SendNotifyLootMoneyRemoved(GetGUID());
            ++itr;
        }
        else
            itr = PlayersLooting.erase(itr);
    }
}

void Loot::generateMoneyLoot(uint32 minAmount, uint32 maxAmount)
{
    if (maxAmount > 0)
    {
        if (maxAmount <= minAmount)
            gold = uint32(maxAmount * sWorld->getRate(RATE_DROP_MONEY));
        else if ((maxAmount - minAmount) < 32700)
            gold = uint32(urand(minAmount, maxAmount) * sWorld->getRate(RATE_DROP_MONEY));
        else
            gold = uint32(urand(minAmount >> 8, maxAmount >> 8) * sWorld->getRate(RATE_DROP_MONEY)) << 8;
    }
}

// Calls processor of corresponding LootTemplate (which handles everything including references)
bool Loot::FillLoot(uint32 lootId, LootStore const& store, Player* lootOwner, bool personal, bool noEmptyError, uint16 lootMode /*= LOOT_MODE_DEFAULT*/, ItemContext context /*= ItemContext::NONE*/)
{
    // Must be provided
    if (!lootOwner)
        return false;

    LootTemplate const* tab = store.GetLootFor(lootId);

    if (!tab)
    {
        if (!noEmptyError)
            TC_LOG_ERROR("sql.sql", "Table '{}' loot id #{} used but it doesn't have records.", store.GetName(), lootId);
        return false;
    }

    _itemContext = context;

    items.reserve(MAX_NR_LOOT_ITEMS);
    quest_items.reserve(MAX_NR_QUEST_ITEMS);

    tab->Process(*this, store.IsRatesAllowed(), lootMode, 0, lootOwner);    // Processing is done there, callback via Loot::AddItem()

    // Setting access rights for group loot case
    Group const* group = lootOwner->GetGroup();
    if (!personal && group)
    {
        roundRobinPlayer = lootOwner->GetGUID();

        for (GroupReference const* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            if (Player const* player = itr->GetSource())    // should actually be looted object instead of lootOwner but looter has to be really close so doesnt really matter
                if (player->IsInMap(lootOwner))
                    FillNotNormalLootFor(player, player->IsAtGroupRewardDistance(lootOwner));

        for (uint8 i = 0; i < items.size(); ++i)
        {
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(items[i].itemid))
                if (proto->GetQuality() < uint32(group->GetLootThreshold()))
                    items[i].is_underthreshold = true;
        }
    }
    // ... for personal loot
    else
        FillNotNormalLootFor(lootOwner, true);

    return true;
}

// Inserts the item into the loot (called by LootTemplate processors)
void Loot::AddItem(LootStoreItem const& item, Player const* player)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item.itemid);
    if (!proto)
        return;

    uint32 count = urand(item.mincount, item.maxcount);
    uint32 stacks = count / proto->GetMaxStackSize() + ((count % proto->GetMaxStackSize()) ? 1 : 0);

    std::vector<LootItem>& lootItems = item.needs_quest ? quest_items : items;
    uint32 limit = item.needs_quest ? MAX_NR_QUEST_ITEMS : MAX_NR_LOOT_ITEMS;

    for (uint32 i = 0; i < stacks && lootItems.size() < limit; ++i)
    {
        LootItem generatedLoot(item);
        generatedLoot.context = _itemContext;
        generatedLoot.count = std::min(count, proto->GetMaxStackSize());
        generatedLoot.itemIndex = lootItems.size();
        if (_itemContext != ItemContext::NONE)
        {
            std::set<uint32> bonusListIDs = sDB2Manager.GetDefaultItemBonusTree(generatedLoot.itemid, _itemContext);
            generatedLoot.BonusListIDs.insert(generatedLoot.BonusListIDs.end(), bonusListIDs.begin(), bonusListIDs.end());
        }

        lootItems.push_back(generatedLoot);
        count -= proto->GetMaxStackSize();

        // In some cases, a dropped item should be visible/lootable only for some players in group
        bool canSeeItemInLootWindow = false;
        if (Group const* group = player->GetGroup())
        {
            for (GroupReference const* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                if (Player const* member = itr->GetSource())
                    if (generatedLoot.AllowedForPlayer(member))
                        canSeeItemInLootWindow = true;
        }
        else if (generatedLoot.AllowedForPlayer(player))
            canSeeItemInLootWindow = true;

        if (!canSeeItemInLootWindow)
            continue;

        // non-conditional one-player only items are counted here,
        // free for all items are counted in FillFFALoot(),
        // non-ffa conditionals are counted in FillNonQuestNonFFAConditionalLoot()
        if (!item.needs_quest && item.conditions.empty() && !proto->HasFlag(ITEM_FLAG_MULTI_DROP))
            ++unlootedCount;
    }
}

LootItem const* Loot::GetItemInSlot(uint32 lootSlot) const
{
    if (lootSlot < items.size())
        return &items[lootSlot];

    lootSlot -= uint32(items.size());
    if (lootSlot < quest_items.size())
        return &quest_items[lootSlot];

    return nullptr;
}

LootItem* Loot::LootItemInSlot(uint32 lootSlot, Player* player, NotNormalLootItem* *qitem, NotNormalLootItem* *ffaitem, NotNormalLootItem* *conditem)
{
    LootItem* item = nullptr;
    bool is_looted = true;
    if (lootSlot >= items.size())
    {
        uint32 questSlot = lootSlot - items.size();
        NotNormalLootItemMap::const_iterator itr = PlayerQuestItems.find(player->GetGUID());
        if (itr != PlayerQuestItems.end() && questSlot < itr->second->size())
        {
            NotNormalLootItem* qitem2 = &(*itr->second)[questSlot];
            if (qitem)
                *qitem = qitem2;
            item = &quest_items[qitem2->index];
            is_looted = qitem2->is_looted;
        }
    }
    else
    {
        item = &items[lootSlot];
        is_looted = item->is_looted;
        if (item->freeforall)
        {
            NotNormalLootItemMap::const_iterator itr = PlayerFFAItems.find(player->GetGUID());
            if (itr != PlayerFFAItems.end())
            {
                for (NotNormalLootItemList::const_iterator iter = itr->second->begin(); iter != itr->second->end(); ++iter)
                    if (iter->index == lootSlot)
                    {
                        NotNormalLootItem* ffaitem2 = (NotNormalLootItem*)&(*iter);
                        if (ffaitem)
                            *ffaitem = ffaitem2;
                        is_looted = ffaitem2->is_looted;
                        break;
                    }
            }
        }
        else if (!item->conditions.empty())
        {
            NotNormalLootItemMap::const_iterator itr = PlayerNonQuestNonFFAConditionalItems.find(player->GetGUID());
            if (itr != PlayerNonQuestNonFFAConditionalItems.end())
            {
                for (NotNormalLootItemList::const_iterator iter = itr->second->begin(); iter != itr->second->end(); ++iter)
                {
                    if (iter->index == lootSlot)
                    {
                        NotNormalLootItem* conditem2 = (NotNormalLootItem*)&(*iter);
                        if (conditem)
                            *conditem = conditem2;
                        is_looted = conditem2->is_looted;
                        break;
                    }
                }
            }
        }
    }

    if (is_looted)
        return nullptr;

    return item;
}

uint32 Loot::GetMaxSlotInLootFor(Player* player) const
{
    NotNormalLootItemMap::const_iterator itr = PlayerQuestItems.find(player->GetGUID());
    return items.size() + (itr != PlayerQuestItems.end() ? itr->second->size() : 0);
}

// return true if there is any item that is lootable for any player (not quest item, FFA or conditional)
bool Loot::hasItemForAll() const
{
    // Gold is always lootable
    if (gold)
        return true;

    for (LootItem const& item : items)
        if (!item.is_looted && !item.freeforall && item.conditions.empty())
            return true;
    return false;
}

// return true if there is any FFA, quest or conditional item for the player.
bool Loot::hasItemFor(Player const* player) const
{
    NotNormalLootItemMap const& lootPlayerQuestItems = GetPlayerQuestItems();
    NotNormalLootItemMap::const_iterator q_itr = lootPlayerQuestItems.find(player->GetGUID());
    if (q_itr != lootPlayerQuestItems.end())
    {
        NotNormalLootItemList* q_list = q_itr->second;
        for (NotNormalLootItemList::const_iterator qi = q_list->begin(); qi != q_list->end(); ++qi)
        {
            const LootItem &item = quest_items[qi->index];
            if (!qi->is_looted && !item.is_looted)
                return true;
        }
    }

    NotNormalLootItemMap const& lootPlayerFFAItems = GetPlayerFFAItems();
    NotNormalLootItemMap::const_iterator ffa_itr = lootPlayerFFAItems.find(player->GetGUID());
    if (ffa_itr != lootPlayerFFAItems.end())
    {
        NotNormalLootItemList* ffa_list = ffa_itr->second;
        for (NotNormalLootItemList::const_iterator fi = ffa_list->begin(); fi != ffa_list->end(); ++fi)
        {
            const LootItem &item = items[fi->index];
            if (!fi->is_looted && !item.is_looted)
                return true;
        }
    }

    NotNormalLootItemMap const& lootPlayerNonQuestNonFFAConditionalItems = GetPlayerNonQuestNonFFAConditionalItems();
    NotNormalLootItemMap::const_iterator nn_itr = lootPlayerNonQuestNonFFAConditionalItems.find(player->GetGUID());
    if (nn_itr != lootPlayerNonQuestNonFFAConditionalItems.end())
    {
        NotNormalLootItemList* conditional_list = nn_itr->second;
        for (NotNormalLootItemList::const_iterator ci = conditional_list->begin(); ci != conditional_list->end(); ++ci)
        {
            const LootItem &item = items[ci->index];
            if (!ci->is_looted && !item.is_looted)
                return true;
        }
    }

    return false;
}

// return true if there is any item over the group threshold (i.e. not underthreshold).
bool Loot::hasOverThresholdItem() const
{
    for (uint8 i = 0; i < items.size(); ++i)
    {
        if (!items[i].is_looted && !items[i].is_underthreshold && !items[i].freeforall)
            return true;
    }

    return false;
}

void Loot::BuildLootResponse(WorldPackets::Loot::LootResponse& packet, Player* viewer, PermissionTypes permission) const
{
    if (permission == NONE_PERMISSION)
        return;

    packet.Coins = gold;

    switch (permission)
    {
        case GROUP_PERMISSION:
        case MASTER_PERMISSION:
        case RESTRICTED_PERMISSION:
        {
            // if you are not the round-robin group looter, you can only see
            // blocked rolled items and quest items, and !ffa items
            for (uint8 i = 0; i < items.size(); ++i)
            {
                if (!items[i].is_looted && !items[i].freeforall && items[i].conditions.empty() && items[i].AllowedForPlayer(viewer))
                {
                    uint8 slot_type;

                    if (items[i].is_blocked) // for ML & restricted is_blocked = !is_underthreshold
                    {
                        switch (permission)
                        {
                            case GROUP_PERMISSION:
                                slot_type = LOOT_SLOT_TYPE_ROLL_ONGOING;
                                break;
                            case MASTER_PERMISSION:
                            {
                                if (viewer->GetGroup() && viewer->GetGroup()->GetMasterLooterGuid() == viewer->GetGUID())
                                    slot_type = LOOT_SLOT_TYPE_MASTER;
                                else
                                    slot_type = LOOT_SLOT_TYPE_LOCKED;
                                break;
                            }
                            case RESTRICTED_PERMISSION:
                                slot_type = LOOT_SLOT_TYPE_LOCKED;
                                break;
                            default:
                                continue;
                        }
                    }
                    else if (roundRobinPlayer.IsEmpty() || viewer->GetGUID() == roundRobinPlayer || !items[i].is_underthreshold)
                    {
                        // no round robin owner or he has released the loot
                        // or it IS the round robin group owner
                        // => item is lootable
                        slot_type = LOOT_SLOT_TYPE_ALLOW_LOOT;
                    }
                    else if (!items[i].rollWinnerGUID.IsEmpty())
                    {
                        if (items[i].rollWinnerGUID == viewer->GetGUID())
                            slot_type = LOOT_SLOT_TYPE_OWNER;
                        else
                            continue;
                    }
                    else
                        // item shall not be displayed.
                        continue;

                    WorldPackets::Loot::LootItemData lootItem;
                    lootItem.LootListID = i + 1;
                    lootItem.UIType = slot_type;
                    lootItem.Quantity = items[i].count;
                    lootItem.Loot.Initialize(items[i]);
                    packet.Items.push_back(lootItem);
                }
            }
            break;
        }
        case ROUND_ROBIN_PERMISSION:
        {
            for (uint8 i = 0; i < items.size(); ++i)
            {
                if (!items[i].is_looted && !items[i].freeforall && items[i].conditions.empty() && items[i].AllowedForPlayer(viewer, false, roundRobinPlayer))
                {
                    if (!roundRobinPlayer.IsEmpty() && viewer->GetGUID() != roundRobinPlayer)
                        // item shall not be displayed.
                        continue;

                    WorldPackets::Loot::LootItemData lootItem;
                    lootItem.LootListID = i + 1;
                    lootItem.UIType = LOOT_SLOT_TYPE_ALLOW_LOOT;
                    lootItem.Quantity = items[i].count;
                    lootItem.Loot.Initialize(items[i]);
                    packet.Items.push_back(lootItem);
                }
            }
            break;
        }
        case ALL_PERMISSION:
        case OWNER_PERMISSION:
        {
            for (uint8 i = 0; i < items.size(); ++i)
            {
                if (!items[i].is_looted && !items[i].freeforall && items[i].conditions.empty() && items[i].AllowedForPlayer(viewer))
                {
                    WorldPackets::Loot::LootItemData lootItem;
                    lootItem.LootListID = i + 1;
                    lootItem.UIType = permission == OWNER_PERMISSION ? LOOT_SLOT_TYPE_OWNER : LOOT_SLOT_TYPE_ALLOW_LOOT;
                    lootItem.Quantity = items[i].count;
                    lootItem.Loot.Initialize(items[i]);
                    packet.Items.push_back(lootItem);
                }
            }
            break;
        }
        default:
            return;
    }

    LootSlotType slotType = permission == OWNER_PERMISSION ? LOOT_SLOT_TYPE_OWNER : LOOT_SLOT_TYPE_ALLOW_LOOT;
    NotNormalLootItemMap const& lootPlayerQuestItems = GetPlayerQuestItems();
    NotNormalLootItemMap::const_iterator q_itr = lootPlayerQuestItems.find(viewer->GetGUID());
    if (q_itr != lootPlayerQuestItems.end())
    {
        NotNormalLootItemList const& q_list = *q_itr->second;
        for (std::size_t i = 0; i < q_list.size(); ++i)
        {
            NotNormalLootItem const& qi = q_list[i];
            LootItem const& item = quest_items[qi.index];
            if (!qi.is_looted && !item.is_looted)
            {
                WorldPackets::Loot::LootItemData lootItem;
                lootItem.LootListID = items.size() + i + 1;
                lootItem.Quantity = item.count;
                lootItem.Loot.Initialize(item);

                if (item.follow_loot_rules)
                {
                    switch (permission)
                    {
                        case MASTER_PERMISSION:
                            lootItem.UIType = LOOT_SLOT_TYPE_MASTER;
                            break;
                        case RESTRICTED_PERMISSION:
                            lootItem.UIType = item.is_blocked ? LOOT_SLOT_TYPE_LOCKED : LOOT_SLOT_TYPE_ALLOW_LOOT;
                            break;
                        case GROUP_PERMISSION:
                        case ROUND_ROBIN_PERMISSION:
                            if (!item.is_blocked)
                                lootItem.UIType = LOOT_SLOT_TYPE_ALLOW_LOOT;
                            else
                                lootItem.UIType = LOOT_SLOT_TYPE_ROLL_ONGOING;
                            break;
                        default:
                            lootItem.UIType = slotType;
                            break;
                    }
                }
                else
                    lootItem.UIType = slotType;

                packet.Items.push_back(lootItem);
            }
        }
    }

    NotNormalLootItemMap const& lootPlayerFFAItems = GetPlayerFFAItems();
    NotNormalLootItemMap::const_iterator ffa_itr = lootPlayerFFAItems.find(viewer->GetGUID());
    if (ffa_itr != lootPlayerFFAItems.end())
    {
        NotNormalLootItemList* ffa_list = ffa_itr->second;
        for (NotNormalLootItemList::const_iterator fi = ffa_list->begin(); fi != ffa_list->end(); ++fi)
        {
            LootItem const& item = items[fi->index];
            if (!fi->is_looted && !item.is_looted)
            {
                WorldPackets::Loot::LootItemData lootItem;
                lootItem.LootListID = fi->index + 1;
                lootItem.UIType = slotType;
                lootItem.Quantity = item.count;
                lootItem.Loot.Initialize(item);
                packet.Items.push_back(lootItem);
            }
        }
    }

    NotNormalLootItemMap const& lootPlayerNonQuestNonFFAConditionalItems = GetPlayerNonQuestNonFFAConditionalItems();
    NotNormalLootItemMap::const_iterator nn_itr = lootPlayerNonQuestNonFFAConditionalItems.find(viewer->GetGUID());
    if (nn_itr != lootPlayerNonQuestNonFFAConditionalItems.end())
    {
        NotNormalLootItemList* conditional_list = nn_itr->second;
        for (NotNormalLootItemList::const_iterator ci = conditional_list->begin(); ci != conditional_list->end(); ++ci)
        {
            LootItem const& item = items[ci->index];
            if (!ci->is_looted && !item.is_looted)
            {
                WorldPackets::Loot::LootItemData lootItem;
                lootItem.LootListID = ci->index + 1;
                lootItem.Quantity = item.count;
                lootItem.Loot.Initialize(item);

                switch (permission)
                {
                    case MASTER_PERMISSION:
                        lootItem.UIType = LOOT_SLOT_TYPE_MASTER;
                        break;
                    case RESTRICTED_PERMISSION:
                        lootItem.UIType = item.is_blocked ? LOOT_SLOT_TYPE_LOCKED : LOOT_SLOT_TYPE_ALLOW_LOOT;
                        break;
                    case GROUP_PERMISSION:
                    case ROUND_ROBIN_PERMISSION:
                        if (!item.is_blocked)
                            lootItem.UIType = LOOT_SLOT_TYPE_ALLOW_LOOT;
                        else
                            lootItem.UIType = LOOT_SLOT_TYPE_ROLL_ONGOING;
                        break;
                    default:
                        lootItem.UIType = slotType;
                        break;
                }

                packet.Items.push_back(lootItem);
            }
        }
    }
}

void Loot::FillNotNormalLootFor(Player const* player, bool presentAtLooting)
{
    ObjectGuid plguid = player->GetGUID();

    NotNormalLootItemMap::const_iterator qmapitr = PlayerQuestItems.find(plguid);
    if (qmapitr == PlayerQuestItems.end())
        FillQuestLoot(player);

    qmapitr = PlayerFFAItems.find(plguid);
    if (qmapitr == PlayerFFAItems.end())
        FillFFALoot(player);

    qmapitr = PlayerNonQuestNonFFAConditionalItems.find(plguid);
    if (qmapitr == PlayerNonQuestNonFFAConditionalItems.end())
        FillNonQuestNonFFAConditionalLoot(player, presentAtLooting);
}

NotNormalLootItemList* Loot::FillFFALoot(Player const* player)
{
    NotNormalLootItemList* ql = new NotNormalLootItemList();

    for (uint8 i = 0; i < items.size(); ++i)
    {
        LootItem &item = items[i];
        if (!item.is_looted && item.freeforall && item.AllowedForPlayer(player))
        {
            ql->push_back(NotNormalLootItem(i));
            ++unlootedCount;
        }
    }
    if (ql->empty())
    {
        delete ql;
        return nullptr;
    }

    PlayerFFAItems[player->GetGUID()] = ql;
    return ql;
}

NotNormalLootItemList* Loot::FillQuestLoot(Player const* player)
{
    if (items.size() == MAX_NR_LOOT_ITEMS)
        return nullptr;

    NotNormalLootItemList* ql = new NotNormalLootItemList();

    for (uint8 i = 0; i < quest_items.size(); ++i)
    {
        LootItem &item = quest_items[i];

        if (!item.is_looted && (item.AllowedForPlayer(player) || (item.follow_loot_rules && player->GetGroup() && ((player->GetGroup()->GetLootMethod() == MASTER_LOOT && player->GetGroup()->GetMasterLooterGuid() == player->GetGUID()) || player->GetGroup()->GetLootMethod() != MASTER_LOOT))))
        {
            ql->push_back(NotNormalLootItem(i));

            // quest items get blocked when they first appear in a
            // player's quest vector
            //
            // increase once if one looter only, looter-times if free for all
            if (item.freeforall || !item.is_blocked)
                ++unlootedCount;
            if (!player->GetGroup() || (player->GetGroup()->GetLootMethod() != GROUP_LOOT && player->GetGroup()->GetLootMethod() != ROUND_ROBIN))
                item.is_blocked = true;

            if (items.size() + ql->size() == MAX_NR_LOOT_ITEMS)
                break;
        }
    }
    if (ql->empty())
    {
        delete ql;
        return nullptr;
    }

    PlayerQuestItems[player->GetGUID()] = ql;
    return ql;
}

NotNormalLootItemList* Loot::FillNonQuestNonFFAConditionalLoot(Player const* player, bool presentAtLooting)
{
    NotNormalLootItemList* ql = new NotNormalLootItemList();

    for (uint8 i = 0; i < items.size(); ++i)
    {
        LootItem &item = items[i];
        if (!item.is_looted && !item.freeforall && (item.AllowedForPlayer(player)))
        {
            if (presentAtLooting)
                item.AddAllowedLooter(player);
            if (!item.conditions.empty())
            {
                ql->push_back(NotNormalLootItem(i));
                if (!item.is_counted)
                {
                    ++unlootedCount;
                    item.is_counted = true;
                }
            }
        }
    }
    if (ql->empty())
    {
        delete ql;
        return nullptr;
    }

    PlayerNonQuestNonFFAConditionalItems[player->GetGUID()] = ql;
    return ql;
}

//
// --------- AELootResult ---------
//

void AELootResult::Add(Item* item, uint8 count, LootType lootType)
{
    auto itr = _byItem.find(item);
    if (itr != _byItem.end())
        _byOrder[itr->second].count += count;
    else
    {
        _byItem[item] = _byOrder.size();
        ResultValue value;
        value.item = item;
        value.count = count;
        value.lootType = lootType;
        _byOrder.push_back(value);
    }
}

AELootResult::OrderedStorage::const_iterator AELootResult::begin() const
{
    return _byOrder.begin();
}

AELootResult::OrderedStorage::const_iterator AELootResult::end() const
{
    return _byOrder.end();
}
