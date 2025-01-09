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

#include "Unit.h"
#include "DB2Stores.h"
#include "Item.h"
#include "Player.h"
#include "Pet.h"
#include "GameTables.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "World.h"
#include <G3D/g3dmath.h>
#include <numeric>

inline bool _ModifyUInt32(bool apply, uint32& baseValue, int32& amount)
{
    // If amount is negative, change sign and value of apply.
    if (amount < 0)
    {
        apply = !apply;
        amount = -amount;
    }
    if (apply)
        baseValue += amount;
    else
    {
        // Make sure we do not get uint32 overflow.
        if (amount > int32(baseValue))
            amount = baseValue;
        baseValue -= amount;
    }
    return apply;
}

/*#######################################
########                         ########
########    UNIT STAT SYSTEM     ########
########                         ########
#######################################*/

void Unit::UpdateAllResistances()
{
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);
}

void Unit::UpdateDamagePhysical(WeaponAttackType attType)
{
    float minDamage = 0.0f;
    float maxDamage = 0.0f;

    CalculateMinMaxDamage(attType, false, true, minDamage, maxDamage);

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UF::UNIT_FIELD_MINDAMAGE, minDamage);
            SetStatFloatValue(UF::UNIT_FIELD_MAXDAMAGE, maxDamage);
            SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MinDamage), minDamage);
            SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MaxDamage), maxDamage);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UF::UNIT_FIELD_MINOFFHANDDAMAGE, minDamage);
            SetStatFloatValue(UF::UNIT_FIELD_MAXOFFHANDDAMAGE, maxDamage);
            SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MinOffHandDamage), minDamage);
            SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MaxOffHandDamage), maxDamage);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UF::UNIT_FIELD_MINRANGEDDAMAGE, minDamage);
            SetStatFloatValue(UF::UNIT_FIELD_MAXRANGEDDAMAGE, maxDamage);
            SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MinRangedDamage), minDamage);
            SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MaxRangedDamage), maxDamage);
            break;
    }
}

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########                         ########
#######################################*/

bool Player::UpdateStats(Stats stat)
{
    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);

    SetStat(stat, int32(value));

    if (stat == STAT_STAMINA || stat == STAT_INTELLECT || stat == STAT_STRENGTH)
    {
        Pet* pet = GetPet();
        if (pet)
            pet->UpdateStats(stat);
    }

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateShieldBlockValue();
            break;
        case STAT_AGILITY:
            UpdateAllCritPercentages();
            UpdateDodgePercentage();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            UpdateSpellCritChance();
            break;
        case STAT_SPIRIT:
            break;
        default:
            break;
    }

    UpdateAttackPowerAndDamage(false);
    UpdateAttackPowerAndDamage(true);
    
    UpdateArmor();
    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();
    return true;
}

void Player::ApplySpellPowerBonus(int32 amount, bool apply)
{
    if (HasAuraType(SPELL_AURA_OVERRIDE_SPELL_POWER_BY_AP_PCT))
        return;

    apply = _ModifyUInt32(apply, m_baseSpellPower, amount);

    // For speed just update for client
    ApplyModUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::ModHealingDonePos), amount, apply);
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        ApplyModDamageDonePos(SpellSchools(i), amount, apply);

    if (HasAuraType(SPELL_AURA_OVERRIDE_ATTACK_POWER_BY_SP_PCT))
    {
        UpdateAttackPowerAndDamage();
        UpdateAttackPowerAndDamage(true);
    }
}

void Player::UpdateSpellDamageAndHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Get healing bonus for all schools
    SetStatInt32Value(UF::ACTIVE_PLAYER_FIELD_MOD_HEALING_DONE_POS, SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
    SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::ModHealingDonePos), SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
    // Get damage bonus for all schools
    Unit::AuraEffectList const& modDamageAuras = GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (uint16 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::ModDamageDoneNeg, i),
            std::accumulate(modDamageAuras.begin(), modDamageAuras.end(), 0, [i](int32 negativeMod, AuraEffect const* aurEff)
        {
            if (aurEff->GetAmount() < 0 && aurEff->GetMiscValue() & (1 << i))
                negativeMod += aurEff->GetAmount();
            return negativeMod;
        }));

        const auto dmg_done_pos = SpellBaseDamageBonusDone(SpellSchoolMask(1 << i)) - m_activePlayerData->ModDamageDoneNeg[i];
        SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::ModDamageDonePos, i), dmg_done_pos);
        SetUInt32Value(UF::ACTIVE_PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, dmg_done_pos);
    }

    if (HasAuraType(SPELL_AURA_OVERRIDE_ATTACK_POWER_BY_SP_PCT))
    {
        UpdateAttackPowerAndDamage();
        UpdateAttackPowerAndDamage(true);
    }
}

bool Player::UpdateAllStats()
{
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        float value = GetTotalStatValue(Stats(i));
        SetStat(Stats(i), int32(value));
    }

    UpdateArmor();
    // calls UpdateAttackPowerAndDamage() in UpdateArmor for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
    UpdateAttackPowerAndDamage(true);
    UpdateMaxHealth();

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllRatings();
    UpdateAllCritPercentages();
    UpdateSpellCritChance();
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
    UpdateShieldBlockValue();
    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();
    UpdateExpertise(BASE_ATTACK);
    UpdateExpertise(OFF_ATTACK);
    RecalculateRating(CR_ARMOR_PENETRATION);
    UpdateAllResistances();

    return true;
}

void Player::ApplySpellPenetrationBonus(int32 amount, bool apply)
{
    ApplyModTargetResistance(-amount, apply);
    m_spellPenetrationItemMod += apply ? amount : -amount;
}

void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        Unit::UpdateResistances(school);

        Pet* pet = GetPet();
        if (pet)
            pet->UpdateResistances(school);
    }
    else
        UpdateArmor();
}

void Player::UpdateArmor()
{
    UnitMods unitMod = UNIT_MOD_ARMOR;

    float value = GetFlatModifierValue(unitMod, BASE_VALUE);    // base armor
    value *= GetPctModifierValue(unitMod, BASE_PCT);            // armor percent
    value += GetStat(STAT_AGILITY) * 2.0f;                      // armor bonus from stats

    // SPELL_AURA_MOD_ARMOR_PCT_FROM_STAT counts as base armor
    GetTotalAuraModifier(SPELL_AURA_MOD_ARMOR_PCT_FROM_STAT, [this, &value](AuraEffect const* aurEff) {
        int32 miscValue = aurEff->GetMiscValue();
        Stats stat = (miscValue != -2) ? Stats(miscValue) : GetPrimaryStat();
        value += CalculatePct(float(GetStat(stat)), aurEff->GetAmount());
        return true;
    });

    float baseValue = value;

    value += GetFlatModifierValue(unitMod, TOTAL_VALUE);        // bonus armor from auras and items

    //add dynamic flat mods
    AuraEffectList const& mResbyIntellect = GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
    for (AuraEffectList::const_iterator i = mResbyIntellect.begin(); i != mResbyIntellect.end(); ++i)
    {
        if ((*i)->GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL)
            value += CalculatePct(GetStat(Stats((*i)->GetMiscValueB())), (*i)->GetAmount());
    }


    value *= GetPctModifierValue(unitMod, TOTAL_PCT);
    value *= GetTotalAuraMultiplier(SPELL_AURA_MOD_BONUS_ARMOR_PCT);

    SetArmor(int32(value), int32(value - baseValue));

    Pet* pet = GetPet();
    if (pet)
        pet->UpdateArmor();

    UpdateAttackPowerAndDamage();                           // armor dependent auras update for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
}

float Player::GetHealthBonusFromStamina() const
{
    float stamina = GetStat(STAT_STAMINA);
    float baseStam = std::min(20.0f, stamina);
    float moreStam = stamina - baseStam;

    return baseStam + (moreStam * 10.0f);
}

Stats Player::GetPrimaryStat() const
{
    uint8 primaryStatPriority = [&]() -> uint8
    {
        if (ChrSpecializationEntry const* specialization = sChrSpecializationStore.LookupEntry(GetPrimarySpecialization()))
            return specialization->PrimaryStatPriority;

        return sChrClassesStore.AssertEntry(GetClass())->PrimaryStatPriority;
    }();

    if (primaryStatPriority >= 4)
        return STAT_STRENGTH;

    if (primaryStatPriority >= 2)
        return STAT_AGILITY;

    return STAT_INTELLECT;
}

float Player::GetManaBonusFromIntellect() const
{
    float intellect = GetStat(STAT_INTELLECT);

    float baseInt = std::min(20.0f, intellect);
    float moreInt = intellect - baseInt;

    return baseInt + (moreInt * 15.0f);
}

void Player::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + GetHealthBonusFromStamina();
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth((uint32)std::max(value, 1.f));
}

uint32 Player::GetPowerIndex(Powers power) const
{
    return sDB2Manager.GetPowerIndexByClass(power, GetClass());
}

void Player::UpdateMaxPower(Powers power)
{
    uint32 powerIndex = GetPowerIndex(power);
    if (powerIndex == MAX_POWERS || powerIndex >= MAX_POWERS_PER_CLASS)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float bonusPower = (power == POWER_MANA && GetCreatePowerValue(power) > 0) ? GetManaBonusFromIntellect() : 0.f;

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreatePowerValue(power);
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + bonusPower;
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, (int32)std::lroundf(value));
}

void Player::ApplyFeralAPBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseFeralAP, amount);
    UpdateAttackPowerAndDamage();
}


void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;
    float level = float(GetLevel());

    ChrClassesEntry const* entry = sChrClassesStore.AssertEntry(GetClass());
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    if (ranged)
    {
        switch (GetClass())
        {
        case CLASS_HUNTER:
            val2 = level * 2.0f + GetStat(STAT_AGILITY) - 10.0f;
            break;
        case CLASS_ROGUE:
            val2 = level + GetStat(STAT_AGILITY) - 10.0f;
            break;
        case CLASS_WARRIOR:
            val2 = level + GetStat(STAT_AGILITY) - 10.0f;
            break;
        case CLASS_DRUID:
            switch (GetShapeshiftForm())
            {
            case FORM_CAT_FORM:
            case FORM_BEAR_FORM:
            case FORM_DIRE_BEAR_FORM:
                val2 = 0.0f; break;
            default:
                val2 = GetStat(STAT_AGILITY) - 10.0f; break;
            }
            break;
        default: val2 = GetStat(STAT_AGILITY) - 10.0f; break;
        }
    }
    else
    {
        switch (GetClass())
        {
        case CLASS_WARRIOR:
            val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
            break;
        case CLASS_PALADIN:
            val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
            break;
        case CLASS_DEATH_KNIGHT:
            val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
            break;
        case CLASS_ROGUE:
            val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY) - 20.0f;
            break;
        case CLASS_HUNTER:
            val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY) - 20.0f;
            break;
        case CLASS_SHAMAN:
            if constexpr (CURRENT_EXPANSION <= EXPANSION_WRATH_OF_THE_LICH_KING)
            {
                val2 = level * 2.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
            }
            else
            {
                val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY) - 20.0f;
            }
            break;
        case CLASS_DRUID:
        {
            // Check if Predatory Strikes is skilled
            float levelBonus = 0.0f;
            float weaponBonus = 0.0f;
            if (IsInFeralForm())
            {
                if (AuraEffect const* levelMod = GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_DRUID, flag128(1563, EFFECT_0)))
                    levelBonus = CalculatePct(1.0f, levelMod->GetAmount());

                if constexpr (CURRENT_EXPANSION >= EXPANSION_WRATH_OF_THE_LICH_KING) {
                    //// = 0 if removing the weapon, do not calculate bonus (uses template)
                    //if (m_baseFeralAP)
                    //{
                    //    if (Item const* weapon = m_items[EQUIPMENT_SLOT_MAINHAND])
                    //    {
                    //        if (AuraEffect const* weaponMod = GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_DRUID, flag128(1563, EFFECT_1)))
                    //        {
                    //            ItemTemplate const* itemTemplate = weapon->GetTemplate();
                    //            int32 bonusAP = itemTemplate->GetTotalAPBonus() + m_baseFeralAP;
                    //            weaponBonus = CalculatePct(static_cast<float>(bonusAP), weaponMod->GetAmount());
                    //        }
                    //    }
                    //}
                }
            }

            switch (GetShapeshiftForm())
            {
            case FORM_CAT_FORM:
                val2 = GetLevel() * levelBonus + GetStat(STAT_STRENGTH) * 2.0f + GetStat(STAT_AGILITY) - 20.0f + weaponBonus + m_baseFeralAP;
                break;
            case FORM_BEAR_FORM:
            case FORM_DIRE_BEAR_FORM:
                val2 = GetLevel() * levelBonus + GetStat(STAT_STRENGTH) * 2.0f - 20.0f + weaponBonus + m_baseFeralAP;
                break;
            case FORM_MOONKIN_FORM:
                val2 = GetStat(STAT_STRENGTH) * 2.0f - 20.0f + m_baseFeralAP;
                break;
            default:
                val2 = GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            }
            break;
        }
        case CLASS_MAGE:
            val2 = GetStat(STAT_STRENGTH) - 10.0f;
            break;
        case CLASS_PRIEST:
            val2 = GetStat(STAT_STRENGTH) - 10.0f;
            break;
        case CLASS_WARLOCK:
            val2 = GetStat(STAT_STRENGTH) - 10.0f;
            break;
        }
    }

    SetStatFlatModifier(unitMod, BASE_VALUE, val2);

    float base_attPower = GetFlatModifierValue(unitMod, BASE_VALUE) * GetPctModifierValue(unitMod, BASE_PCT);
    float attPowerMod = GetFlatModifierValue(unitMod, TOTAL_VALUE);
    float attPowerMultiplier = GetPctModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE) {
        //add dynamic flat mods
        if (ranged)
        {
            if ((GetClassMask() & CLASSMASK_WAND_USERS) == 0)
            {
                AuraEffectList const& mRAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT);
                for (AuraEffect const* aurEff : mRAPbyStat)
                    attPowerMod += CalculatePct(GetStat(Stats(aurEff->GetMiscValue())), aurEff->GetAmount());
            }
        }
        else
        {
            AuraEffectList const& mAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT);
            for (AuraEffect const* aurEff : mAPbyStat)
                attPowerMod += CalculatePct(GetStat(Stats(aurEff->GetMiscValue())), aurEff->GetAmount());
        }
    }

    if constexpr (CURRENT_EXPANSION >= EXPANSION_WRATH_OF_THE_LICH_KING) {
        // applies to both, amount updated in PeriodicTick each 30 seconds
        attPowerMod += GetTotalAuraModifier(SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR);
    }

    if (ranged)
    {
        SetRangedAttackPower(int32(base_attPower));
        SetRangedAttackPowerModPos(int32(attPowerMod));
        SetRangedAttackPowerMultiplier(attPowerMultiplier);
    }
    else
    {
        SetAttackPower(int32(base_attPower));
        SetAttackPowerModPos(int32(attPowerMod));
        SetAttackPowerMultiplier(attPowerMultiplier);
    }

    Pet* pet = GetPet();                                //update pet's AP
    Guardian* guardian = GetGuardianPet();
    //automatically update weapon damage after attack power modification
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);
        if (pet && pet->IsHunterPet()) // At ranged attack change for hunter pet
            pet->UpdateAttackPowerAndDamage();
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (Item* offhand = GetWeaponForAttack(OFF_ATTACK, true))
            if (CanDualWield() || offhand->GetTemplate()->HasFlag(ITEM_FLAG3_ALWAYS_ALLOW_DUAL_WIELD))
                UpdateDamagePhysical(OFF_ATTACK);

        if (HasAuraType(SPELL_AURA_OVERRIDE_SPELL_POWER_BY_AP_PCT))
            UpdateSpellDamageAndHealingBonus();

        if (pet && pet->IsPetGhoul()) // At melee attack power change for DK pet
            pet->UpdateAttackPowerAndDamage();

        if (guardian && guardian->IsSpiritWolf()) // At melee attack power change for Shaman feral spirit
            guardian->UpdateAttackPowerAndDamage();
    }
}

void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage) const
{
    UnitMods unitMod;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    float attackPowerMod = std::max(GetAPMultiplier(attType, normalized), 0.25f);

    float baseValue  = GetFlatModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType, false) / 14.0f * attackPowerMod;
    float basePct    = GetPctModifierValue(unitMod, BASE_PCT);
    float totalValue = GetFlatModifierValue(unitMod, TOTAL_VALUE);
    float totalPct   = addTotalPct ? GetPctModifierValue(unitMod, TOTAL_PCT) : 1.0f;

    float weaponMinDamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weaponMaxDamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    SpellShapeshiftFormEntry const* shapeshift = sSpellShapeshiftFormStore.LookupEntry(GetShapeshiftForm());
    if (shapeshift && shapeshift->CombatRoundTime)
    {
        weaponMinDamage = weaponMinDamage * shapeshift->CombatRoundTime / 1000.0f / attackPowerMod;
        weaponMaxDamage = weaponMaxDamage * shapeshift->CombatRoundTime / 1000.0f / attackPowerMod;
    }
    else if (!CanUseAttackType(attType)) // check if player not in form but still can't use (disarm case)
    {
        // cannot use ranged/off attack, set values to 0
        if (attType != BASE_ATTACK)
        {
            minDamage = 0;
            maxDamage = 0;
            return;
        }
        weaponMinDamage = BASE_MINDAMAGE;
        weaponMaxDamage = BASE_MAXDAMAGE;
    }
    else if (attType == RANGED_ATTACK)
    {
        weaponMinDamage += GetAmmoDPS() * attackPowerMod;
        weaponMaxDamage += GetAmmoDPS() * attackPowerMod;
    }

    minDamage = ((weaponMinDamage + baseValue) * basePct + totalValue) * totalPct;
    maxDamage = ((weaponMaxDamage + baseValue) * basePct + totalValue) * totalPct;
}

void Player::UpdateDefenseBonusesMod()
{
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
}

void Player::UpdateBlockPercentage()
{
    // No block
    float value = 0.0f;
    if (CanBlock())
    {
        // Base value
        value = 5.0f;
        // Modify value from defense skill
        value += (int32(GetDefenseSkillValue()) - int32(GetMaxSkillValueForLevel())) * 0.04f;
        // Increase from SPELL_AURA_MOD_BLOCK_PERCENT aura
        value += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
        if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
        {
            // Increase from rating
            value += GetRatingBonusValue(CR_BLOCK);
        }

        if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
             value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_BLOCK) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_BLOCK) : value;
    }
    SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::BlockPercentage), value);
}

void Player::UpdateCritPercentage(WeaponAttackType attType)
{
    auto calcValue = [&](BaseModGroup modGroup, CombatRating cr)
    {
        // flat = bonus from crit auras, pct = bonus from agility, combat rating = mods from items
        float value = GetBaseModValue(modGroup, FLAT_MOD) + GetBaseModValue(modGroup, PCT_MOD);

        if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
        {
            value += GetRatingBonusValue(cr);
        }

        // Modify crit from weapon skill and maximized defense skill of same level victim difference
        value += (int32(GetWeaponSkillValue(attType)) - int32(GetMaxSkillValueForLevel())) * 0.04f;

        if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
            value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_CRIT) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_CRIT) : value;
        

        return std::max(0.f, value);
    };


    switch (attType)
    {
    case OFF_ATTACK:
    {
        const auto crit = calcValue(OFFHAND_CRIT_PERCENTAGE, CR_CRIT_MELEE);
        SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::OffhandCritPercentage), crit);
        SetFloatValue(UF::ACTIVE_PLAYER_FIELD_OFFHAND_CRIT_PERCENTAGE, crit);
    }
        break;
    case RANGED_ATTACK:
    {
        const auto crit = calcValue(RANGED_CRIT_PERCENTAGE, CR_CRIT_RANGED);
        SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::RangedCritPercentage), crit);
        SetFloatValue(UF::ACTIVE_PLAYER_FIELD_RANGED_CRIT_PERCENTAGE, crit);
    }
        break;
    case BASE_ATTACK:
    default:
    {
        const auto crit = calcValue(CRIT_PERCENTAGE, CR_CRIT_MELEE);
        SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::CritPercentage), crit);
        SetFloatValue(UF::ACTIVE_PLAYER_FIELD_CRIT_PERCENTAGE, crit);
    }
        break;
    }
}

void Player::UpdateAllCritPercentages()
{
    float value = GetMeleeCritFromAgility();

    SetBaseModPctValue(CRIT_PERCENTAGE, value);
    SetBaseModPctValue(OFFHAND_CRIT_PERCENTAGE, value);
    SetBaseModPctValue(RANGED_CRIT_PERCENTAGE, value);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

void Player::UpdateMastery()
{
    if (!CanUseMastery())
    {
        SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::Mastery), 0.0f);
        SetFloatValue(UF::ACTIVE_PLAYER_FIELD_MASTERY, 0.0f);
        return;
    }

    float value = GetTotalAuraModifier(SPELL_AURA_MASTERY);
    value += GetRatingBonusValue(CR_MASTERY);
    SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::Mastery), value);
    SetFloatValue(UF::ACTIVE_PLAYER_FIELD_MASTERY, value);

    ChrSpecializationEntry const* chrSpec = sChrSpecializationStore.LookupEntry(GetPrimarySpecialization());
    if (!chrSpec)
        return;

    for (int32 masterySpellId : chrSpec->MasterySpellID)
    {
        if (Aura* aura = GetAura(masterySpellId))
        {
            for (AuraEffect* auraEff : aura->GetAuraEffects())
            {
                float mult = auraEff->GetSpellEffectInfo().BonusCoefficient;
                if (G3D::fuzzyEq(mult, 0.0f))
                    continue;

                auraEff->ChangeAmount(int32(value * mult));
            }
        }
    }
}

void Player::UpdateVersatilityDamageDone()
{
    // No proof that CR_VERSATILITY_DAMAGE_DONE is allways = ActivePlayerData::Versatility
    SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::Versatility), m_activePlayerData->CombatRatings[CR_VERSATILITY_DAMAGE_DONE]);

    if (GetClass() == CLASS_HUNTER)
        UpdateDamagePhysical(RANGED_ATTACK);
    else
        UpdateDamagePhysical(BASE_ATTACK);
}

void Player::UpdateHealingDonePercentMod()
{
    float value = 1.0f;

    for (AuraEffect const* auraEffect : GetAuraEffectsByType(SPELL_AURA_MOD_HEALING_DONE_PERCENT))
        AddPct(value, auraEffect->GetAmount());

    SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::ModHealingDonePercent), (int32)value);
}

float const m_diminishing_k[MAX_CLASSES] =
{
    0.9560f,  // Warrior
    0.9560f,  // Paladin
    0.9880f,  // Hunter
    0.9880f,  // Rogue
    0.9830f,  // Priest
    0.9560f,  // DK
    0.9880f,  // Shaman
    0.9830f,  // Mage
    0.9830f,  // Warlock
    0.9830f,  // Monk
    0.9720f,  // Druid
};

// helper function
float CalculateDiminishingReturns(float const (&capArray)[MAX_CLASSES], uint8 playerClass, float nonDiminishValue, float diminishValue)
{
    if constexpr (CURRENT_EXPANSION <= EXPANSION_WRATH_OF_THE_LICH_KING)
    {
        return diminishValue + nonDiminishValue;
    }

    //  1     1     k              cx
    // --- = --- + --- <=> x' = --------
    //  x'    c     x            x + ck

    // where:
    // k  is m_diminishing_k for that class
    // c  is capArray for that class
    // x  is chance before DR (diminishValue)
    // x' is chance after DR (our result)

    uint32 const classIdx = playerClass - 1;

    float const k = m_diminishing_k[classIdx];
    float const c = capArray[classIdx];

    float result = c * diminishValue / (diminishValue + c * k);
    result += nonDiminishValue;
    return result;
}

float const parry_cap[MAX_CLASSES] =
{
    65.631440f,     // Warrior
    65.631440f,     // Paladin
    145.560408f,    // Hunter
    145.560408f,    // Rogue
    0.0f,           // Priest
    65.631440f,     // DK
    145.560408f,    // Shaman
    0.0f,           // Mage
    0.0f,           // Warlock
    90.6425f,       // Monk
    0.0f,           // Druid
};

void Player::UpdateParryPercentage()
{
    // No parry
    float value = 0.0f;
    uint32 pclass = GetClass() - 1;
    if (CanParry() && parry_cap[pclass] > 0.0f)
    {
        float nondiminishing = 5.0f;
        float diminishing = 0.f;

        // Modify value from defense skill (only bonus from defense rating diminishes)
        nondiminishing += (int32(GetSkillValue(SKILL_DEFENSE)) - int32(GetMaxSkillValueForLevel())) * 0.04f;
        // Parry from SPELL_AURA_MOD_PARRY_PERCENT aura
        nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);

        if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
        {
            diminishing += (GetRatingBonusValue(CR_DEFENSE_SKILL) * 0.04f);
            // Parry from rating
            diminishing += GetRatingBonusValue(CR_PARRY);
        }

        // apply diminishing formula to diminishing parry chance
        value = CalculateDiminishingReturns(parry_cap, GetClass(), nondiminishing, diminishing);

        if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
             value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_PARRY) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_PARRY) : value;

        value = std::max(0.f, value);
    }
    SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::ParryPercentage), value);
}

float const dodge_cap[MAX_CLASSES] =
{
    65.631440f,     // Warrior
    65.631440f,     // Paladin
    145.560408f,    // Hunter
    145.560408f,    // Rogue
    150.375940f,    // Priest
    65.631440f,     // DK
    145.560408f,    // Shaman
    150.375940f,    // Mage
    150.375940f,    // Warlock
    145.560408f,    // Monk
    116.890707f,    // Druid
};

void Player::UpdateDodgePercentage()
{
    float diminishing = 0.0f, nondiminishing = 0.0f;
    GetDodgeFromAgility(diminishing, nondiminishing);
    // Modify value from defense skill (only bonus from defense rating diminishes)
    nondiminishing += (int32(GetSkillValue(SKILL_DEFENSE)) - int32(GetMaxSkillValueForLevel())) * 0.04f;
    // Dodge from SPELL_AURA_MOD_DODGE_PERCENT aura
    nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);

    if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
    {
        diminishing += (GetRatingBonusValue(CR_DEFENSE_SKILL) * 0.04f);
        // Dodge from rating
        diminishing += GetRatingBonusValue(CR_DODGE);
    }

    // apply diminishing formula to diminishing dodge chance
    float value = CalculateDiminishingReturns(dodge_cap, GetClass(), nondiminishing, diminishing);

    if (sWorld->getBoolConfig(CONFIG_STATS_LIMITS_ENABLE))
         value = value > sWorld->getFloatConfig(CONFIG_STATS_LIMITS_DODGE) ? sWorld->getFloatConfig(CONFIG_STATS_LIMITS_DODGE) : value;

    SetUpdateFieldStatValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::DodgePercentage), value);
}

void Player::UpdateSpellCritChance()
{
    float crit = 5.0f;
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit from SPELL_AURA_MOD_CRIT_PCT
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PCT);

    if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
    {
        // Increase crit from spell crit ratings
        crit += GetRatingBonusValue(CR_CRIT_SPELL);
    }

    // Store crit value
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i) {
        // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
        const float school_crit = crit + GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1 << i);

        SetFloatValue(UF::ACTIVE_PLAYER_FIELD_SPELL_CRIT_PERCENTAGE1 + i, school_crit);
        SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::SpellCritPercentage, i), school_crit);
    }
}

void Player::UpdateArmorPenetration(int32 amount)
{
    // Store Rating Value
    SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::CombatRatings, CR_ARMOR_PENETRATION), amount);
}

void Player::UpdateMeleeHitChances()
{
    //TODOFROST
    m_modMeleeHitChance = 7.5f + GetRatingBonusValue(CR_HIT_MELEE);
}

void Player::UpdateRangedHitChances()
{
    //TODOFROST
    m_modRangedHitChance = 7.5f + GetRatingBonusValue(CR_HIT_RANGED);
}

void Player::UpdateSpellHitChances()
{
    m_modSpellHitChance = (float)GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE);
    if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
    {
        m_modSpellHitChance += GetRatingBonusValue(CR_HIT_SPELL);
    }
}

void Player::UpdateExpertise(WeaponAttackType attack)
{
    //TODOFROST
    if (attack == RANGED_ATTACK)
        return;

    int32 expertise = int32(GetRatingBonusValue(CR_EXPERTISE));

    Item const* weapon = GetWeaponForAttack(attack, true);
    expertise += GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE, [weapon](AuraEffect const* aurEff) -> bool
    {
        return aurEff->GetSpellInfo()->IsItemFitToSpellRequirements(weapon);
    });

    if (expertise < 0)
        expertise = 0;

    switch (attack)
    {
        case BASE_ATTACK:
            SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::MainhandExpertise), expertise);
            SetUInt32Value(UF::ACTIVE_PLAYER_FIELD_EXPERTISE, expertise);
            break;
        case OFF_ATTACK:
            SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::OffhandExpertise), expertise);
            SetUInt32Value(UF::ACTIVE_PLAYER_FIELD_OFFHAND_EXPERTISE, expertise);
            break;
        default:
            break;
    }
}

void Player::ApplyManaRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseManaRegen, amount);
    UpdateManaRegen();
}

void Player::ApplyHealthRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseHealthRegen, amount);
}

void Player::UpdateManaRegen()
{
    uint32 manaIndex = GetPowerIndex(POWER_MANA);
    if (manaIndex == MAX_POWERS)
        return;

    float power_regen = OCTRegenMPPerSpirit();

    if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
    {
        float Intellect = GetStat(STAT_INTELLECT);
        // Mana regen from spirit and intellect
        power_regen *= std::sqrt(Intellect);
    }

    // Apply PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit base regen
    power_regen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

    // Mana regen from SPELL_AURA_MOD_POWER_REGEN aura
    float power_regen_mp5 = (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + m_baseManaRegen) / 5.0f;

    // SPELL_AURA_ADD_FLAT_MODIFIER_BY_SPELL_LABEL is the proper name, needs proper implementation
    // Get bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura
    if constexpr (CURRENT_EXPANSION >= EXPANSION_THE_BURNING_CRUSADE)
    {
        AuraEffectList const& regenAura = GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
        for (AuraEffectList::const_iterator i = regenAura.begin(); i != regenAura.end(); ++i)
            power_regen_mp5 += GetStat(Stats((*i)->GetMiscValue())) * (*i)->GetAmount() / 500.0f;
    }

    // Set regen rate in cast state apply only on spirit based regen
    int32 modManaRegenInterrupt = GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
    if (modManaRegenInterrupt > 100)
        modManaRegenInterrupt = 100;

    SetUpdateFieldValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::PowerRegenInterruptedFlatModifier, manaIndex), power_regen_mp5 + CalculatePct(power_regen, modManaRegenInterrupt));
    SetStatFloatValue(UF::UNIT_FIELD_MOD_POWER_REGEN + manaIndex, power_regen_mp5 + power_regen);
    SetUpdateFieldValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::PowerRegenFlatModifier, manaIndex), power_regen_mp5 + power_regen);
}

void Player::UpdateAllRunesRegen()
{
    if (GetClass() != CLASS_DEATH_KNIGHT)
        return;

    uint32 runeIndex = GetPowerIndex(POWER_RUNES);
    if (runeIndex == MAX_POWERS)
        return;

    PowerTypeEntry const* runeEntry = sDB2Manager.GetPowerTypeEntry(POWER_RUNES);

    uint32 cooldown = GetRuneBaseCooldown();
    SetStatFloatValue(UF::UNIT_FIELD_MOD_POWER_REGEN + runeIndex, float(1 * IN_MILLISECONDS) / float(cooldown) - runeEntry->RegenPeace);
    SetUpdateFieldValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::PowerRegenFlatModifier, runeIndex), float(1 * IN_MILLISECONDS) / float(cooldown) - runeEntry->RegenPeace);
}

void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraStatMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraStatMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Creature::UpdateStats(Stats /*stat*/)
{
    return true;
}

bool Creature::UpdateAllStats()
{
    UpdateMaxHealth();
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();

    return true;
}

void Creature::UpdateArmor()
{
    float baseValue = GetFlatModifierValue(UNIT_MOD_ARMOR, BASE_VALUE);
    if constexpr (CURRENT_EXPANSION <= EXPANSION_THE_BURNING_CRUSADE)
    {
        baseValue += GetStat(STAT_AGILITY) * 2.f;
    }
    float value = GetTotalAuraModValue(UNIT_MOD_ARMOR);
    SetArmor(int32(baseValue), int32(value - baseValue));
}

void Creature::UpdateMaxHealth()
{
    float value = GetTotalAuraModValue(UNIT_MOD_HEALTH);
    SetMaxHealth(uint32(value));
}

uint32 Creature::GetPowerIndex(Powers power) const
{
    if (power == GetPowerType())
        return 0;
    if (power == POWER_ALTERNATE_POWER)
        return 1;
    if (power == POWER_COMBO_POINTS)
        return 2;
    return MAX_POWERS;
}

void Creature::UpdateMaxPower(Powers power)
{
    if (GetPowerIndex(power) == MAX_POWERS)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreatePowerValue(power);
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE);
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, (int32)std::lroundf(value));
}

void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    //TODOFROST
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    float baseAttackPower       = GetFlatModifierValue(unitMod, BASE_VALUE) * GetPctModifierValue(unitMod, BASE_PCT);
    float attackPowerMultiplier = GetPctModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    if (ranged)
    {
        SetRangedAttackPower(int32(baseAttackPower));
        SetRangedAttackPowerMultiplier(attackPowerMultiplier);
    }
    else
    {
        SetAttackPower(int32(baseAttackPower));
        SetAttackPowerMultiplier(attackPowerMultiplier);
    }

    // automatically update weapon damage after attack power modification
    if (ranged)
        UpdateDamagePhysical(RANGED_ATTACK);
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        UpdateDamagePhysical(OFF_ATTACK);
    }
}

void Creature::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage) const
{
    float variance = 1.0f;
    UnitMods unitMod;
    switch (attType)
    {
        case BASE_ATTACK:
        default:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            variance = GetCreatureTemplate()->RangeVariance;
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    if (attType == OFF_ATTACK && !haveOffhandWeapon())
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;
        return;
    }

    float weaponMinDamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weaponMaxDamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    if (!CanUseAttackType(attType)) // disarm case
    {
        weaponMinDamage = 0.0f;
        weaponMaxDamage = 0.0f;
    }

    float attackPower      = GetTotalAttackPowerValue(attType, false);
    float attackSpeedMulti = GetAPMultiplier(attType, normalized);
    float baseValue        = GetFlatModifierValue(unitMod, BASE_VALUE) + (attackPower / 14.0f) * variance;
    float basePct          = GetPctModifierValue(unitMod, BASE_PCT) * attackSpeedMulti;
    float totalValue       = GetFlatModifierValue(unitMod, TOTAL_VALUE);
    float totalPct         = addTotalPct ? GetPctModifierValue(unitMod, TOTAL_PCT) : 1.0f;
    float dmgMultiplier    = GetCreatureTemplate()->ModDamage; // = ModDamage * _GetDamageMod(rank);

    minDamage = ((weaponMinDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;
    maxDamage = ((weaponMaxDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########                         ########
#######################################*/

#define ENTRY_IMP               416
#define ENTRY_VOIDWALKER        1860
#define ENTRY_SUCCUBUS          1863
#define ENTRY_FELHUNTER         417
#define ENTRY_FELGUARD          17252
#define ENTRY_WATER_ELEMENTAL   510
#define ENTRY_TREANT            1964
#define ENTRY_FIRE_ELEMENTAL    15438
#define ENTRY_GHOUL             26125
#define ENTRY_BLOODWORM         28017

bool Guardian::UpdateStats(Stats stat)
{
    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);
    UpdateStatBuffMod(stat);
    float ownersBonus = 0.0f;

    Unit* owner = GetOwner();
    // Handle Death Knight Glyphs and Talents
    float mod = 0.75f;
    if (IsPetGhoul() && (stat == STAT_STAMINA || stat == STAT_STRENGTH))
    {
        if (stat == STAT_STAMINA)
            mod = 0.3f; // Default Owner's Stamina scale
        else
            mod = 0.7f; // Default Owner's Strength scale

        ownersBonus = float(owner->GetStat(stat)) * mod;
        value += ownersBonus;
    }
    else if (stat == STAT_STAMINA)
    {
        ownersBonus = CalculatePct(owner->GetStat(STAT_STAMINA), 30);
        value += ownersBonus;
    }
                                                            //warlock's and mage's pets gain 30% of owner's intellect
    else if (stat == STAT_INTELLECT)
    {
        if (owner->GetClass() == CLASS_WARLOCK || owner->GetClass() == CLASS_MAGE)
        {
            ownersBonus = CalculatePct(owner->GetStat(stat), 30);
            value += ownersBonus;
        }
    }
/*
    else if (stat == STAT_STRENGTH)
    {
        if (IsPetGhoul())
            value += float(owner->GetStat(stat)) * 0.3f;
    }
*/

    SetStat(stat, int32(value));
    m_statFromOwner[stat] = ownersBonus;
    UpdateStatBuffMod(stat);

    switch (stat)
    {
        case STAT_STRENGTH:         UpdateAttackPowerAndDamage();        break;
        case STAT_AGILITY:          UpdateArmor();                       break;
        case STAT_STAMINA:          UpdateMaxHealth();                   break;
        case STAT_INTELLECT:        UpdateMaxPower(POWER_MANA);          break;
        case STAT_SPIRIT:
        default:
            break;
    }

    return true;
}

bool Guardian::UpdateAllStats()
{
    UpdateMaxHealth();

    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        UpdateStats(Stats(i));

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();

    return true;
}

void Guardian::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float baseValue = GetFlatModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + school), BASE_VALUE);

        // hunter and warlock pets gain 40% of owner's resistance
        if (IsPet())
            baseValue += float(CalculatePct(m_owner->GetResistance(SpellSchools(school)), 40));

        SetResistance(SpellSchools(school), int32(baseValue));
    }
    else
        UpdateArmor();
}

void Guardian::UpdateArmor()
{
    float baseValue = 0.0f;
    float value = 0.0f;
    float bonus_armor = 0.0f;
    UnitMods unitMod = UNIT_MOD_ARMOR;

    // hunter pets gain 35% of owner's armor value, warlock pets gain 100% of owner's armor
    if (IsHunterPet())
        bonus_armor = float(CalculatePct(m_owner->GetArmor(), 70));
    else if (IsPet())
        bonus_armor = m_owner->GetArmor();

    value  = GetFlatModifierValue(unitMod, BASE_VALUE);
    baseValue = value;
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + bonus_armor;
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    SetArmor(int32(baseValue), int32(value - baseValue));
}

void Guardian::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;
    float stamina = GetStat(STAT_STAMINA) - GetCreateStat(STAT_STAMINA);

    float multiplicator;
    switch (GetEntry())
    {
        case ENTRY_IMP:         multiplicator = 8.4f;   break;
        case ENTRY_VOIDWALKER:  multiplicator = 11.0f;  break;
        case ENTRY_SUCCUBUS:    multiplicator = 9.1f;   break;
        case ENTRY_FELHUNTER:   multiplicator = 9.5f;   break;
        case ENTRY_FELGUARD:    multiplicator = 11.0f;  break;
        case ENTRY_BLOODWORM:   multiplicator = 1.0f;   break;
        default:                multiplicator = 10.0f;  break;
    }

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + stamina * multiplicator;
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth((uint32)value);
}

void Guardian::UpdateMaxPower(Powers power)
{
    if (GetPowerIndex(power) == MAX_POWERS)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreatePowerValue(power);
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE);
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, int32(value));
}

void Guardian::UpdateAttackPowerAndDamage(bool ranged)
{
    if (ranged)
        return;

    float val = 0.0f;
    float bonusAP = 0.0f;
    UnitMods unitMod = UNIT_MOD_ATTACK_POWER;

    if (GetEntry() == ENTRY_IMP)                                   // imp's attack power
        val = GetStat(STAT_STRENGTH) - 10.0f;
    else
        val = 2 * GetStat(STAT_STRENGTH) - 20.0f;

    Player* owner = GetOwner() ? GetOwner()->ToPlayer() : nullptr;
    if (owner)
    {
        if (IsHunterPet())                      //hunter pets benefit from owner's attack power
        {
            float mod = 1.0f;                                                 //Hunter contribution modifier
            bonusAP = owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.22f * mod;
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.1287f * mod));
        }
        else if (IsPetGhoul()) //ghouls benefit from deathknight's attack power (may be summon pet or not)
        {
            bonusAP = owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.22f;
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.1287f));
        }
        else if (IsSpiritWolf()) //wolf benefit from shaman's attack power
        {
            float dmg_multiplier = 0.31f;
            bonusAP = owner->GetTotalAttackPowerValue(BASE_ATTACK) * dmg_multiplier;
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(BASE_ATTACK) * dmg_multiplier));
        }
        //demons benefit from warlocks shadow or fire damage
        else if (IsPet())
        {
            int32 fire = owner->m_activePlayerData->ModDamageDonePos[SPELL_SCHOOL_FIRE] - owner->m_activePlayerData->ModDamageDoneNeg[SPELL_SCHOOL_FIRE];
            int32 shadow = owner->m_activePlayerData->ModDamageDonePos[SPELL_SCHOOL_SHADOW] - owner->m_activePlayerData->ModDamageDoneNeg[SPELL_SCHOOL_SHADOW];
            int32 maximum  = (fire > shadow) ? fire : shadow;
            if (maximum < 0)
                maximum = 0;
            SetBonusDamage(int32(maximum * 0.15f));
            bonusAP = maximum * 0.57f;
        }
        //water elementals benefit from mage's frost damage
        else if (GetEntry() == ENTRY_WATER_ELEMENTAL)
        {
            int32 frost = owner->m_activePlayerData->ModDamageDonePos[SPELL_SCHOOL_FROST] - owner->m_activePlayerData->ModDamageDoneNeg[SPELL_SCHOOL_FROST];
            if (frost < 0)
                frost = 0;
            SetBonusDamage(int32(frost * 0.4f));
        }
    }

    SetStatFlatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, val + bonusAP);

    //in BASE_VALUE of UNIT_MOD_ATTACK_POWER for creatures we store data of meleeattackpower field in DB
    float base_attPower  = GetFlatModifierValue(unitMod, BASE_VALUE) * GetPctModifierValue(unitMod, BASE_PCT);
    float attPowerMultiplier = GetPctModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    SetAttackPower(int32(base_attPower));
    SetAttackPowerMultiplier(attPowerMultiplier);

    //automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
}

void Guardian::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK)
        return;

    float bonusDamage = 0.0f;
    if (Player* playerOwner = m_owner->ToPlayer())
    {
        //force of nature
        if (GetEntry() == ENTRY_TREANT)
        {
            int32 spellDmg = playerOwner->m_activePlayerData->ModDamageDonePos[SPELL_SCHOOL_NATURE] - playerOwner->m_activePlayerData->ModDamageDoneNeg[SPELL_SCHOOL_NATURE];
            if (spellDmg > 0)
                bonusDamage = spellDmg * 0.09f;
        }
        //greater fire elemental
        else if (GetEntry() == ENTRY_FIRE_ELEMENTAL)
        {
            int32 spellDmg = playerOwner->m_activePlayerData->ModDamageDonePos[SPELL_SCHOOL_FIRE] - playerOwner->m_activePlayerData->ModDamageDoneNeg[SPELL_SCHOOL_FIRE];
            if (spellDmg > 0)
                bonusDamage = spellDmg * 0.4f;
        }
    }

    UnitMods unitMod = UNIT_MOD_DAMAGE_MAINHAND;

    float att_speed = float(GetBaseAttackTime(BASE_ATTACK))/1000.0f;

    float base_value  = GetFlatModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType, false) / 3.5f * att_speed + bonusDamage;
    float base_pct    = GetPctModifierValue(unitMod, BASE_PCT);
    float total_value = GetFlatModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetPctModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);

    float mindamage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    float maxdamage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;

    SetStatFloatValue(UF::UNIT_FIELD_MINDAMAGE, mindamage);
    SetStatFloatValue(UF::UNIT_FIELD_MAXDAMAGE, maxdamage);
    SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MinDamage), mindamage);
    SetUpdateFieldStatValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::MaxDamage), maxdamage);
}

void Guardian::SetBonusDamage(int32 damage)
{
    m_bonusSpellDamage = damage;
    if (Player* playerOwner = GetOwner()->ToPlayer())
        playerOwner->SetPetSpellPower(damage);
}
