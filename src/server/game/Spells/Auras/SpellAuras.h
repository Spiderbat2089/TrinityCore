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

#ifndef TRINITY_SPELLAURAS_H
#define TRINITY_SPELLAURAS_H

#include "EnumFlag.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "Unit.h"

class SpellInfo;
struct SpellModifier;
struct ProcTriggerSpell;
struct SpellProcEntry;

namespace WorldPackets
{
    namespace Spells
    {
        struct AuraInfo;
    }
}

// forward decl
class AuraEffect;
class Aura;
class DynamicObject;
class AuraScript;
class ProcInfo;
class ChargeDropEvent;

// update aura target map every 500 ms instead of every update - reduce amount of grid searcher calls
#define UPDATE_TARGET_MAP_INTERVAL 500

class TC_GAME_API AuraApplication
{
    friend void Unit::_ApplyAura(AuraApplication * aurApp, uint8 effMask);
    friend void Unit::_UnapplyAura(AuraApplicationMap::iterator &i, AuraRemoveFlags removeMode);
    friend void Unit::_ApplyAuraEffect(Aura* aura, uint8 effIndex);
    friend void Unit::RemoveAura(AuraApplication * aurApp, AuraRemoveFlags mode);
    friend AuraApplication * Unit::_CreateAuraApplication(Aura* aura, uint8 effMask);
    private:
        Unit* const _target;
        Aura* const _base;
        EnumFlag<AuraRemoveFlags> _removeMode;         // Store info for know remove aura reason
        uint8 _slot;                                   // Aura slot on unit
        uint8 _flags;                                  // Aura info flag
        uint8 _effectsToApply;                         // Used only at spell hit to determine which effect should be applied
        bool _needClientUpdate:1;

        explicit AuraApplication(Unit* target, Unit* caster, Aura* base, uint8 effMask);
        void _Remove();
    private:
        void _InitFlags(Unit* caster, uint8 effMask);
        void _HandleEffect(uint8 effIndex, bool apply);
    public:

        Unit* GetTarget() const { return _target; }
        Aura* GetBase() const { return _base; }

        uint8 GetSlot() const { return _slot; }
        uint8 GetFlags() const { return _flags; }
        uint8 GetEffectMask() const { return _flags & (AFLAG_EFF_INDEX_0 | AFLAG_EFF_INDEX_1 | AFLAG_EFF_INDEX_2); }
        bool HasEffect(uint8 effect) const { ASSERT(effect < MAX_SPELL_EFFECTS);  return (_flags & (1 << effect)) != 0; }
        bool IsPositive() const { return (_flags & AFLAG_POSITIVE) != 0; }
        bool IsSelfcast() const { return (_flags & AFLAG_CASTER) != 0; }
        uint8 GetEffectsToApply() const { return _effectsToApply; }

        void SetRemoveMode(AuraRemoveFlags mode) { _removeMode = mode; }
        EnumFlag<AuraRemoveFlags> GetRemoveMode() const { return _removeMode; }

        void SetNeedClientUpdate() { _needClientUpdate = true;}
        bool IsNeedClientUpdate() const { return _needClientUpdate;}
        void BuildUpdatePacket(WorldPackets::Spells::AuraInfo& auraInfo, bool remove) const;
        void ClientUpdate(bool remove = false);
};

class TC_GAME_API Aura
{
    friend Aura* Unit::_TryStackingOrRefreshingExistingAura(SpellInfo const* newAura, uint8 effMask, Unit* caster, int32 *baseAmount, Item* castItem, ObjectGuid casterGUID);
    public:
        typedef std::map<ObjectGuid, AuraApplication*> ApplicationMap;

        static uint8 BuildEffectMaskForOwner(SpellInfo const* spellProto, uint8 availableEffectMask, WorldObject* owner);
        static Aura* TryRefreshStackOrCreate(SpellInfo const* spellproto, uint8 tryEffMask, WorldObject* owner, Unit* caster, int32* baseAmount = nullptr, Item* castItem = nullptr, ObjectGuid casterGUID = ObjectGuid::Empty, bool* refresh = nullptr);
        static Aura* TryCreate(SpellInfo const* spellproto, uint8 effMask, WorldObject* owner, Unit* caster, int32* baseAmount = nullptr, Item* castItem = nullptr, ObjectGuid casterGUID = ObjectGuid::Empty);
        static Aura* Create(SpellInfo const* spellproto, uint8 effMask, WorldObject* owner, Unit* caster, int32* baseAmount, Item* castItem, ObjectGuid casterGUID);
        explicit Aura(SpellInfo const* spellproto, WorldObject* owner, Unit* caster, Item* castItem, ObjectGuid casterGUID);
        void _InitEffects(uint8 effMask, Unit* caster, int32 *baseAmount);
        virtual ~Aura();

        SpellInfo const* GetSpellInfo() const { return m_spellInfo; }
        uint32 GetId() const{ return GetSpellInfo()->Id; }

        ObjectGuid GetCastItemGUID() const { return m_castItemGuid; }
        ObjectGuid GetCasterGUID() const { return m_casterGuid; }
        Unit* GetCaster() const;
        WorldObject* GetOwner() const { return m_owner; }
        Unit* GetUnitOwner() const { ASSERT(GetType() == UNIT_AURA_TYPE); return (Unit*)m_owner; }
        DynamicObject* GetDynobjOwner() const { ASSERT(GetType() == DYNOBJ_AURA_TYPE); return (DynamicObject*)m_owner; }

        AuraObjectType GetType() const;

        virtual void _ApplyForTarget(Unit* target, Unit* caster, AuraApplication * auraApp);
        virtual void _UnapplyForTarget(Unit* target, Unit* caster, AuraApplication * auraApp);
        void _Remove(AuraRemoveFlags removeMode);
        virtual void Remove(AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault) = 0;

        virtual void FillTargetMap(std::unordered_map<Unit*, uint8>& targets, Unit* caster) = 0;
        void UpdateTargetMap(Unit* caster, bool apply = true);

        void _RegisterForTargets() {Unit* caster = GetCaster(); UpdateTargetMap(caster, false);}
        void ApplyForTargets() {Unit* caster = GetCaster(); UpdateTargetMap(caster, true);}
        void _ApplyEffectForTargets(uint8 effIndex);

        void UpdateOwner(uint32 diff, WorldObject* owner);
        void Update(uint32 diff, Unit* caster);

        time_t GetApplyTime() const { return m_applyTime; }
        int32 GetMaxDuration() const { return m_maxDuration; }
        void SetMaxDuration(int32 duration) { m_maxDuration = duration; }
        int32 CalcMaxDuration() const { return CalcMaxDuration(GetCaster()); }
        int32 CalcMaxDuration(Unit* caster) const;
        int32 GetDuration() const { return m_duration; }
        void SetDuration(int32 duration, bool withMods = false);
        int32_t GetRolledOverDuration() const { return m_rolledOverDuration; }
        void RefreshDuration(bool withMods = false);
        void RefreshTimers();
        bool IsExpired() const { return !GetDuration() && !m_dropEvent; }
        bool IsPermanent() const { return GetMaxDuration() == -1; }

        uint8 GetCharges() const { return m_procCharges; }
        void SetCharges(uint8 charges);
        uint8 CalcMaxCharges(Unit* caster) const;
        uint8 CalcMaxCharges() const { return CalcMaxCharges(GetCaster()); }
        bool ModCharges(int32 num, AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault);
        bool DropCharge(AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault) { return ModCharges(-1, removeMode); }
        void ModChargesDelayed(int32 num, AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault);
        void DropChargeDelayed(uint32 delay, AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault);

        uint8 GetStackAmount() const { return m_stackAmount; }
        void SetStackAmount(uint8 num);
        bool ModStackAmount(int32 num, AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault);

        uint8 GetCasterLevel() const { return m_casterLevel; }

        bool HasMoreThanOneEffectForType(AuraType auraType) const;
        bool IsArea() const;
        bool IsPassive() const;
        bool IsDeathPersistent() const;

        bool IsRemovedOnShapeLost(Unit* target) const
        {
            return GetCasterGUID() == target->GetGUID()
                    && m_spellInfo->Stances
                    && !m_spellInfo->HasAttribute(SPELL_ATTR2_NOT_NEED_SHAPESHIFT)
                    && !m_spellInfo->HasAttribute(SPELL_ATTR0_NOT_SHAPESHIFT);
        }

        bool CanBeSaved() const;
        bool IsRemoved() const { return m_isRemoved; }
        bool CanBeSentToClient() const;
        // Limited cast aura helpers
        bool IsLimitedTarget() const {return m_isLimitedTarget; }
        bool IsLimitedTargetWith(Aura const* aura) const;
        void SetIsLimitedTarget(bool val) { m_isLimitedTarget = val; }
        void UnregisterLimitedTarget();
        int32 CalcDispelChance(Unit const* auraTarget, bool offensive) const;

        void SetLoadedState(int32 maxduration, int32 duration, int32 charges, uint8 stackamount, uint8 recalculateMask, int32 * amount);

        // helpers for aura effects
        bool HasEffect(uint8 effIndex) const { return GetEffect(effIndex) != nullptr; }
        bool HasEffectType(AuraType type) const;
        static bool EffectTypeNeedsSendingAmount(AuraType type);
        AuraEffect* GetEffect(uint8 effIndex) const { ASSERT (effIndex < MAX_SPELL_EFFECTS); return m_effects[effIndex]; }
        uint8 GetEffectMask() const { uint8 effMask = 0; for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i) if (m_effects[i]) effMask |= 1<<i; return effMask; }
        void RecalculateAmountOfEffects();
        void HandleAllEffects(AuraApplication * aurApp, uint8 mode, bool apply);

        // Helpers for targets
        ApplicationMap const& GetApplicationMap() { return m_applications; }
        void GetApplicationList(Unit::AuraApplicationList& applicationList) const;
        const AuraApplication* GetApplicationOfTarget(ObjectGuid guid) const { ApplicationMap::const_iterator itr = m_applications.find(guid); if (itr != m_applications.end()) return itr->second; return nullptr; }
        AuraApplication* GetApplicationOfTarget(ObjectGuid guid) { ApplicationMap::iterator itr = m_applications.find(guid); if (itr != m_applications.end()) return itr->second; return nullptr; }
        bool IsAppliedOnTarget(ObjectGuid guid) const { return m_applications.find(guid) != m_applications.end(); }

        void SetNeedClientUpdateForTargets() const;
        void HandleAuraSpecificMods(AuraApplication const* aurApp, Unit* caster, bool apply, bool onReapply);
        bool CanBeAppliedOn(Unit* target);
        bool CheckAreaTarget(Unit* target);
        bool CanStackWith(Aura const* existingAura) const;

        bool IsProcOnCooldown(std::chrono::steady_clock::time_point now) const;
        void AddProcCooldown(std::chrono::steady_clock::time_point cooldownEnd);
        void ResetProcCooldown();
        bool IsUsingCharges() const { return m_isUsingCharges; }
        void SetUsingCharges(bool val) { m_isUsingCharges = val; }
        void PrepareProcToTrigger(AuraApplication* aurApp, ProcEventInfo& eventInfo, std::chrono::steady_clock::time_point now);
        uint8 GetProcEffectMask(AuraApplication* aurApp, ProcEventInfo& eventInfo, std::chrono::steady_clock::time_point now) const;
        float CalcProcChance(SpellProcEntry const& procEntry, ProcEventInfo& eventInfo) const;
        void TriggerProcOnEvent(uint8 procEffectMask, AuraApplication* aurApp, ProcEventInfo& eventInfo);

        // AuraScript
        void LoadScripts();
        bool CallScriptCheckAreaTargetHandlers(Unit* target);
        void CallScriptDispel(DispelInfo* dispelInfo);
        void CallScriptAfterDispel(DispelInfo* dispelInfo);
        bool CallScriptEffectApplyHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp, AuraEffectHandleModes mode);
        bool CallScriptEffectRemoveHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp, AuraEffectHandleModes mode);
        void CallScriptAfterEffectApplyHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp, AuraEffectHandleModes mode);
        void CallScriptAfterEffectRemoveHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp, AuraEffectHandleModes mode);
        bool CallScriptEffectPeriodicHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp);
        void CallScriptEffectUpdatePeriodicHandlers(AuraEffect* aurEff);
        void CallScriptEffectCalcAmountHandlers(AuraEffect const* aurEff, int32 & amount, bool & canBeRecalculated);
        void CallScriptEffectCalcPeriodicHandlers(AuraEffect const* aurEff, bool & isPeriodic, int32 & amplitude);
        void CallScriptEffectCalcSpellModHandlers(AuraEffect const* aurEff, SpellModifier* & spellMod);
        void CallScriptEffectAbsorbHandlers(AuraEffect* aurEff, AuraApplication const* aurApp, DamageInfo & dmgInfo, uint32 & absorbAmount, bool & defaultPrevented);
        void CallScriptEffectAfterAbsorbHandlers(AuraEffect* aurEff, AuraApplication const* aurApp, DamageInfo & dmgInfo, uint32 & absorbAmount);
        void CallScriptEffectManaShieldHandlers(AuraEffect* aurEff, AuraApplication const* aurApp, DamageInfo & dmgInfo, uint32 & absorbAmount, bool & defaultPrevented);
        void CallScriptEffectAfterManaShieldHandlers(AuraEffect* aurEff, AuraApplication const* aurApp, DamageInfo & dmgInfo, uint32 & absorbAmount);
        void CallScriptEffectSplitHandlers(AuraEffect* aurEff, AuraApplication const* aurApp, DamageInfo & dmgInfo, uint32 & splitAmount);
        // Spell Proc Hooks
        bool CallScriptCheckProcHandlers(AuraApplication const* aurApp, ProcEventInfo& eventInfo);
        bool CallScriptCheckEffectProcHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp, ProcEventInfo& eventInfo);
        bool CallScriptPrepareProcHandlers(AuraApplication const* aurApp, ProcEventInfo& eventInfo);
        bool CallScriptProcHandlers(AuraApplication const* aurApp, ProcEventInfo& eventInfo);
        void CallScriptAfterProcHandlers(AuraApplication const* aurApp, ProcEventInfo& eventInfo);
        bool CallScriptEffectProcHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp, ProcEventInfo& eventInfo);
        void CallScriptAfterEffectProcHandlers(AuraEffect const* aurEff, AuraApplication const* aurApp, ProcEventInfo& eventInfo);

        template <class Script>
        Script* GetScript(std::string const& scriptName) const
        {
            return dynamic_cast<Script*>(GetScriptByName(scriptName));
        }

        std::vector<AuraScript*> m_loadedScripts;

    private:
        AuraScript* GetScriptByName(std::string const& scriptName) const;
        void _DeleteRemovedApplications();

    protected:
        SpellInfo const* const m_spellInfo;
        ObjectGuid const m_casterGuid;
        ObjectGuid const m_castItemGuid;                    // it is NOT safe to keep a pointer to the item because it may get deleted
        time_t const m_applyTime;
        WorldObject* const m_owner;

        int32 m_maxDuration;                                // Max aura duration
        int32 m_duration;                                   // Current time
        int32 m_rolledOverDuration;                         // Duration remainder, rolled over on refresh, if the spell does not reset its periodic timer.
                                                            // This is normally the time remaining until the next tick of the dot when refreshed.
        int32 m_timeCla;                                    // Timer for power per sec calcultion
        int32 m_updateTargetMapInterval;                    // Timer for UpdateTargetMapOfEffect

        uint8 const m_casterLevel;                          // Aura level (store caster level for correct show level dep amount)
        uint8 m_procCharges;                                // Aura charges (0 for infinite)
        uint8 m_stackAmount;                                // Aura stack amount

        AuraEffect* m_effects[3];
        ApplicationMap m_applications;

        bool m_isRemoved:1;
        bool m_isLimitedTarget:1;                        // true if it's a limited target spell and registered at caster - can change at spell steal for example
        bool m_isUsingCharges:1;

        ChargeDropEvent* m_dropEvent;

        std::chrono::steady_clock::time_point m_procCooldown;

    private:
        Unit::AuraApplicationList m_removedApplications;
};

class TC_GAME_API UnitAura : public Aura
{
    friend Aura* Aura::Create(SpellInfo const* spellproto, uint8 effMask, WorldObject* owner, Unit* caster, int32 *baseAmount, Item* castItem, ObjectGuid casterGUID);
    protected:
        explicit UnitAura(SpellInfo const* spellproto, uint8 effMask, WorldObject* owner, Unit* caster, int32 *baseAmount, Item* castItem, ObjectGuid casterGUID);
    public:
        void _ApplyForTarget(Unit* target, Unit* caster, AuraApplication * aurApp) override;
        void _UnapplyForTarget(Unit* target, Unit* caster, AuraApplication * aurApp) override;

        void Remove(AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault) override;

        void FillTargetMap(std::unordered_map<Unit*, uint8>& targets, Unit* caster) override;

        // Allow Apply Aura Handler to modify and access m_AuraDRGroup
        void SetDiminishGroup(DiminishingGroup group) { m_AuraDRGroup = group; }
        DiminishingGroup GetDiminishGroup() const { return m_AuraDRGroup; }

    private:
        DiminishingGroup m_AuraDRGroup;               // Diminishing
};

class TC_GAME_API DynObjAura : public Aura
{
    friend Aura* Aura::Create(SpellInfo const* spellproto, uint8 effMask, WorldObject* owner, Unit* caster, int32 *baseAmount, Item* castItem, ObjectGuid casterGUID);
    protected:
        explicit DynObjAura(SpellInfo const* spellproto, uint8 effMask, WorldObject* owner, Unit* caster, int32 *baseAmount, Item* castItem, ObjectGuid casterGUID);
    public:
        void Remove(AuraRemoveFlags removeMode = AuraRemoveFlags::ByDefault) override;

        void FillTargetMap(std::unordered_map<Unit*, uint8>& targets, Unit* caster) override;
};

class TC_GAME_API ChargeDropEvent : public BasicEvent
{
    friend class Aura;
    protected:
        ChargeDropEvent(Aura* base, AuraRemoveFlags mode) : _base(base), _mode(mode) { }
        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override;

    private:
        Aura* _base;
        AuraRemoveFlags _mode;
};
#endif
