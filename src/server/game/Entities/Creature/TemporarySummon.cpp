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

#include "TemporarySummon.h"
#include "CreatureAI.h"
#include "DBCStructure.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"

TempSummon::TempSummon(SummonPropertiesEntry const* properties, Unit* owner, bool isWorldObject) :
    Creature(isWorldObject), m_Properties(properties), m_type(TEMPSUMMON_MANUAL_DESPAWN),
    m_timer(0), m_lifetime(0)
{
    m_unitTypeMask |= UNIT_MASK_SUMMON;

    if (owner)
        m_summonerGUID = owner->GetGUID();

}

Unit* TempSummon::GetSummoner() const
{
    return m_summonerGUID ? ObjectAccessor::GetUnit(*this, m_summonerGUID) : nullptr;
}

Creature* TempSummon::GetSummonerCreatureBase() const
{
    return m_summonerGUID ? ObjectAccessor::GetCreature(*this, m_summonerGUID) : nullptr;
}

void TempSummon::Update(uint32 diff)
{
    Creature::Update(diff);

    if (m_deathState == DEAD)
    {
        UnSummon();
        return;
    }
    switch (m_type)
    {
        case TEMPSUMMON_MANUAL_DESPAWN:
        case TEMPSUMMON_DEAD_DESPAWN:
            break;
        case TEMPSUMMON_TIMED_DESPAWN:
        {
            if (m_timer <= diff)
            {
                UnSummon();
                return;
            }

            m_timer -= diff;
            break;
        }
        case TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT:
        {
            if (!IsInCombat())
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }

                m_timer -= diff;
            }
            else if (m_timer != m_lifetime)
                m_timer = m_lifetime;

            break;
        }

        case TEMPSUMMON_CORPSE_TIMED_DESPAWN:
        {
            if (m_deathState == CORPSE)
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }

                m_timer -= diff;
            }
            break;
        }
        case TEMPSUMMON_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped
            if (m_deathState == CORPSE)
            {
                UnSummon();
                return;
            }

            break;
        }
        case TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN:
        {
            if (m_deathState == CORPSE)
            {
                UnSummon();
                return;
            }

            if (!IsInCombat())
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }
                else
                    m_timer -= diff;
            }
            else if (m_timer != m_lifetime)
                m_timer = m_lifetime;
            break;
        }
        case TEMPSUMMON_TIMED_OR_DEAD_DESPAWN:
        {
            if (!IsInCombat() && IsAlive())
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }
                else
                    m_timer -= diff;
            }
            else if (m_timer != m_lifetime)
                m_timer = m_lifetime;
            break;
        }
        default:
            UnSummon();
            TC_LOG_ERROR("entities.unit", "Temporary summoned creature (entry: %u) have unknown type %u of ", GetEntry(), m_type);
            break;
    }
}

void TempSummon::InitializeBeforeAddToMap(uint32 duration)
{
    ASSERT(!IsPet());

    m_timer = duration;
    m_lifetime = duration;

    if (m_type == TEMPSUMMON_MANUAL_DESPAWN)
        m_type = (duration == 0) ? TEMPSUMMON_DEAD_DESPAWN : TEMPSUMMON_TIMED_DESPAWN;

    Unit* owner = GetSummoner();

    // Possessed summons
    if (owner && IsTrigger() && m_spells[0])
        if (owner->GetTypeId() == TYPEID_PLAYER)
            m_ControlledByPlayer = true;

    if (!m_Properties)
        return;

    // Initialize faction
    if (owner && m_Properties->GetFlags().HasFlag(SummonPropertiesFlags::UseSummonerFaction))
        SetFaction(owner->GetFaction());
    else if (m_Properties->Faction)
        SetFaction(m_Properties->Faction);

    // Initialize level
    if (owner && !m_Properties->GetFlags().HasFlag(SummonPropertiesFlags::UseCreatureLevel))
        SetLevel(owner->getLevel());

    // Summons inherit their guild guid from their summoner, even if the summon has been summoned by another creature
    if (owner && owner->GetGuidValue(OBJECT_FIELD_DATA))
    {
        ObjectGuid guildGUID = owner->GetGuidValue(OBJECT_FIELD_DATA);
        if (!guildGUID.IsEmpty())
        {
            SetGuidValue(OBJECT_FIELD_DATA, owner->GetGuidValue(OBJECT_FIELD_DATA));
            SetUInt16Value(OBJECT_FIELD_TYPE, 1, 1); // Has guild data
        }
    }
}

void TempSummon::InitializeAfterAddToMap()
{
    if (Unit* owner = GetSummoner())
    {
        if (owner->GetTypeId() == TYPEID_UNIT && owner->ToCreature()->IsAIEnabled)
            owner->ToCreature()->AI()->JustSummoned(this);
        if (IsAIEnabled)
            AI()->IsSummonedBy(owner);
    }
}

void TempSummon::UpdateObjectVisibilityOnCreate()
{
    WorldObject::UpdateObjectVisibility(true);
}

void TempSummon::SetTempSummonType(TempSummonType type)
{
    m_type = type;
}

void TempSummon::UnSummon(uint32 msTime)
{
    if (msTime)
    {
        ForcedUnsummonDelayEvent* pEvent = new ForcedUnsummonDelayEvent(*this);

        m_Events.AddEvent(pEvent, m_Events.CalculateTime(msTime));
        return;
    }

    //ASSERT(!IsPet());
    if (IsPet())
    {
        ((Pet*)this)->Remove(PET_SAVE_DISMISS);
        ASSERT(!IsInWorld());
        return;
    }

    Unit* owner = GetSummoner();
    if (owner)
    {
        if (owner->GetTypeId() == TYPEID_UNIT && owner->ToCreature()->IsAIEnabled)
            owner->ToCreature()->AI()->SummonedCreatureDespawn(this);
    }

    AddObjectToRemoveList();
}

bool ForcedUnsummonDelayEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    m_owner.UnSummon();
    return true;
}

void TempSummon::RemoveFromWorld()
{
    if (!IsInWorld())
        return;

    if (m_Properties)
    {
        int32 slot = m_Properties->Slot;
        if (slot > 0)
            if (Unit* owner = GetSummoner())
                if (owner->m_SummonSlot[slot] == GetGUID())
                    owner->m_SummonSlot[slot].Clear();
    }

    //if (GetOwnerGUID())
    //    TC_LOG_ERROR("entities.unit", "Unit %u has owner guid when removed from world", GetEntry());

    Creature::RemoveFromWorld();
}

Guardian::Guardian(SummonPropertiesEntry const* properties, Unit* owner, bool isWorldObject) : TempSummon(properties, owner, isWorldObject)
{
    m_unitTypeMask |= UNIT_MASK_GUARDIAN;
}

void Guardian::InitializeBeforeAddToMap(uint32 duration)
{
    TempSummon::InitializeBeforeAddToMap(duration);

    if (Unit* summoner = GetSummoner())
        SetCreatorGUID(summoner->GetGUID());


}

void Guardian::InitializeAfterAddToMap()
{
    TempSummon::InitializeAfterAddToMap();
}
