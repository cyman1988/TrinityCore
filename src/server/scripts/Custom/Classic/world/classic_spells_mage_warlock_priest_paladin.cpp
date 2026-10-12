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
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Classic 1.60 mage, warlock, priest and paladin spells whose client data is a dummy (spell effect, aura or proc trigger of a "Dummy
// Trigger" spell) with no server side behaviour. The vanilla behaviour follows VMaNGOS (spell_mage.cpp, spell_priest.cpp,
// spell_paladin.cpp, UnitAuraProcHandler.cpp) and the 1.60 tooltips. Talent amounts come from the trait curves, so they are read from
// the talent aura effects, never hardcoded.

#include "ScriptMgr.h"
#include "AreaTrigger.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

namespace
{
// Base mana cost of a spell (flat cost plus the percentage of base mana), before cost modifiers
int32 GetClassicBaseManaCostMWPP(SpellInfo const* spellInfo, Unit const* caster)
{
    for (SpellPowerEntry const* power : spellInfo->PowerCosts)
    {
        if (!power || power->PowerType != POWER_MANA)
            continue;

        int32 cost = power->ManaCost;
        if (power->PowerCostPct > 0.0f)
            cost += int32(CalculatePct(float(caster->GetCreateMana()), power->PowerCostPct));
        return cost;
    }
    return 0;
}

bool ClassicSpellNameIs(SpellInfo const* spellInfo, std::initializer_list<char const*> names)
{
    if (!spellInfo || !spellInfo->SpellName)
        return false;

    std::string_view name = (*spellInfo->SpellName)[DEFAULT_LOCALE];
    for (char const* candidate : names)
        if (name == candidate)
            return true;
    return false;
}
}

/*######
## Mage
######*/

enum ClassicMageSpells
{
    SPELL_MAGE_COLD_SNAP                = 12472,
    SPELL_MAGE_MASTER_OF_ELEMENTS_MANA  = 29077,
    SPELL_MAGE_COMBUSTION               = 11129,
    SPELL_MAGE_COMBUSTION_CRIT          = 28682
};

// 12472 - Cold Snap: "Finishes the remaining cooldown on all your other Frost spells." (VMaNGOS spell_mage.cpp: mage spells with the
// Frost school and a cooldown). The retail script is for 235219. Only the mage's own Frost spells (spell book) are reset.
class classic_spell_mage_cold_snap : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* mage = GetCaster()->ToPlayer();
        if (!mage)
            return;

        mage->GetSpellHistory()->ResetCooldowns([mage](SpellHistory::CooldownEntry const& cooldown)
        {
            if (cooldown.SpellId == SPELL_MAGE_COLD_SNAP || cooldown.ItemId || !mage->HasSpell(cooldown.SpellId))
                return false;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(cooldown.SpellId, DIFFICULTY_NONE);
            return spellInfo && (spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_FROST);
        }, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(classic_spell_mage_cold_snap::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 29074 - Master of Elements: "Your Fire and Frost critical strikes will refund X% of their base mana cost." (29077, spell_proc: fire and
// frost harmful spells, critical hits only; VMaNGOS UnitAuraProcHandler.cpp)
class classic_spell_mage_master_of_elements : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_MASTER_OF_ELEMENTS_MANA });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        _mana = 0;
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!spellInfo || !(eventInfo.GetHitMask() & PROC_HIT_CRITICAL))
            return false;

        if (AuraEffect const* talent = GetEffect(EFFECT_0))
            _mana = CalculatePct(GetClassicBaseManaCostMWPP(spellInfo, GetTarget()), talent->GetAmountAsInt());
        return _mana > 0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_MAGE_MASTER_OF_ELEMENTS_MANA, CastSpellExtraArgs(aurEff).AddSpellMod(SPELLVALUE_BASE_POINT0, _mana));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_mage_master_of_elements::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_master_of_elements::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    int32 _mana = 0;
};

// 11129 - Combustion: "When activated, this spell causes each of your Fire damage spell hits to increase your critical strike chance with
// Fire damage spells by 10%. This effect lasts until you have caused 3 non-periodic critical strikes with Fire spells." Each Fire hit adds
// a stack of 28682 (effect 1 casts the first one); only critical hits use a charge (3 in the aura data), and the last one ends the crit
// buff (VMaNGOS spell_mage.cpp)
class classic_spell_mage_combustion : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_COMBUSTION_CRIT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        return damageInfo && damageInfo->GetDamage() && eventInfo.GetSpellInfo()
            && (eventInfo.GetSpellInfo()->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE);
    }

    // only critical hits use a charge
    void PrepareProc(ProcEventInfo& eventInfo)
    {
        if (!(eventInfo.GetHitMask() & PROC_HIT_CRITICAL))
            PreventDefaultAction();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        // the crit buff was dispelled or clicked off: Combustion ends
        if (!GetTarget()->HasAura(SPELL_MAGE_COMBUSTION_CRIT))
        {
            GetAura()->Remove();
            return;
        }

        // last critical hit: the aura is removed with its last charge (and the crit buff with it)
        if ((eventInfo.GetHitMask() & PROC_HIT_CRITICAL) && GetAura()->IsUsingCharges() && !GetAura()->GetCharges())
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_MAGE_COMBUSTION_CRIT, CastSpellExtraArgs(aurEff));
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_MAGE_COMBUSTION_CRIT);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_mage_combustion::CheckProc);
        DoPrepareProc += AuraProcFn(classic_spell_mage_combustion::PrepareProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_combustion::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_mage_combustion::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 28682 - Combustion (crit buff): clicking it off ends Combustion (VMaNGOS spell_mage.cpp)
class classic_spell_mage_combustion_crit : public AuraScript
{
    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_CANCEL)
            GetTarget()->RemoveAurasDueToSpell(SPELL_MAGE_COMBUSTION);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_mage_combustion_crit::HandleRemove, EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
    }
};

/*######
## Warlock
######*/

enum ClassicWarlockSpells
{
    SPELL_WARL_SHADOW_TRANCE    = 17941,
    SPELL_WARL_PYROCLASM        = 18073,
    SPELL_WARL_PYROCLASM_STUN   = 18093
};

// 18094 - Nightfall: "Gives your Corruption, Drain Soul, Drain Life, and Wrack spells a X% chance to cause you to enter a Shadow Trance
// after damaging the opponent." Every periodic damage tick of those spells (spell_proc) rolls the talent amount for 17941.
class classic_spell_warl_nightfall : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARL_SHADOW_TRANCE });
    }

    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        return damageInfo && damageInfo->GetDamage()
            && ClassicSpellNameIs(eventInfo.GetSpellInfo(), { "Corruption", "Drain Soul", "Drain Life", "Wrack" })
            && roll_chance(float(aurEff->GetAmount()));
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_WARL_SHADOW_TRANCE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_warl_nightfall::CheckProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(classic_spell_warl_nightfall::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 18073 - Pyroclasm: "Gives your Soul Fire spell a X% chance to Stun the target for 3 sec, and your Rain of Fire and Hellfire spells a X%
// chance over their duration to Stun targets they damage for 3 sec." Effect 0 is the chance; effect 1 only triggers the "Dummy Trigger"
// 18350 (disabled in spell_proc). Hellfire's 15 ticks each roll a fifteenth of the chance (VMaNGOS UnitAuraProcHandler.cpp); Rain of Fire
// is handled on its tick (classic_spell_warl_pyroclasm_rain_of_fire).
class classic_spell_warl_pyroclasm : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARL_PYROCLASM_STUN });
    }

    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!target || target == GetTarget() || !target->IsAlive() || !damageInfo || !damageInfo->GetDamage())
            return false;

        float chance = float(aurEff->GetAmount());
        if (ClassicSpellNameIs(eventInfo.GetSpellInfo(), { "Hellfire Effect" }))
            chance /= 15.0f;
        else if (!ClassicSpellNameIs(eventInfo.GetSpellInfo(), { "Soul Fire" }))
            return false;

        return roll_chance(chance);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_WARL_PYROCLASM_STUN, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_warl_pyroclasm::CheckProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(classic_spell_warl_pyroclasm::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 5740, 6219, 11677, 11678 - Rain of Fire (extra script) for 18073 Pyroclasm: the ground effect ticks through
// classic_spell_ground_area_damage (4 ticks, no proc); this second script on the same tick gives every enemy inside a quarter of the
// Pyroclasm chance to be stunned (18093)
class classic_spell_warl_pyroclasm_rain_of_fire : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARL_PYROCLASM_STUN });
    }

    void HandleTick(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        AuraEffect const* pyroclasm = caster->GetAuraEffect(SPELL_WARL_PYROCLASM, EFFECT_0);
        if (!pyroclasm || pyroclasm->GetAmount() <= 0)
            return;

        GuidUnorderedSet targets;
        for (AreaTrigger* areaTrigger : caster->GetAreaTriggers(GetId()))
            targets.insert(areaTrigger->GetInsideUnits().begin(), areaTrigger->GetInsideUnits().end());

        for (ObjectGuid const& guid : targets)
        {
            Unit* target = ObjectAccessor::GetUnit(*caster, guid);
            if (!target || !target->IsAlive() || !caster->IsValidAttackTarget(target, GetSpellInfo()))
                continue;

            if (roll_chance(float(pyroclasm->GetAmount()) / 4.0f))
                caster->CastSpell(target, SPELL_WARL_PYROCLASM_STUN, CastSpellExtraArgs(aurEff));
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_warl_pyroclasm_rain_of_fire::HandleTick, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
    }
};

/*######
## Priest
######*/

enum ClassicPriestSpells
{
    SPELL_PRI_WEAKENED_SOUL         = 6788,
    SPELL_PRI_SPIRIT_OF_REDEMPTION  = 20711,
    SPELL_PRI_SPIRIT_OF_REDEMPTION_FORM = 27827
};

// 15237, 15430, 15431, 27799, 27800, 27801 - Holy Nova: "Causes an explosion of holy light around the caster, causing X Holy damage to
// all enemy targets within 10 yards and healing all party members within 10 yards for Y." The 1.60 spell only has the damage; the heal
// is the rank's 23455, 23458, 23459, 27803, 27804, 27805 (party members around the caster, VMaNGOS spell_priest.cpp)
class classic_spell_pri_holy_nova : public SpellScript
{
    static uint32 GetHealSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 15237: return 23455;
            case 15430: return 23458;
            case 15431: return 23459;
            case 27799: return 27803;
            case 27800: return 27804;
            case 27801: return 27805;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return GetHealSpell(spellInfo->Id) && ValidateSpellInfo({ GetHealSpell(spellInfo->Id) });
    }

    void HandleAfterCast()
    {
        GetCaster()->CastSpell(GetCaster(), GetHealSpell(GetSpellInfo()->Id), GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_spell_pri_holy_nova::HandleAfterCast);
    }
};

// 17, 592, 600, 3747, 6065, 6066, 10898, 10899, 10900, 10901 - Power Word: Shield: "Once shielded, the target cannot be shielded again for
// 15 sec." The 1.60 spell only has the absorb; the shield puts Weakened Soul (6788, immunity to the shield mechanic) on the target
// (VMaNGOS spell_priest.cpp), and a target with Weakened Soul cannot be shielded.
class classic_spell_pri_power_word_shield : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRI_WEAKENED_SOUL });
    }

    SpellCastResult CheckCast()
    {
        if (Unit* target = GetExplTargetUnit())
            if (target->HasAura(SPELL_PRI_WEAKENED_SOUL))
                return SPELL_FAILED_IMMUNE;
        return SPELL_CAST_OK;
    }

    void HandleAfterHit()
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_PRI_WEAKENED_SOUL, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_pri_power_word_shield::CheckCast);
        AfterHit += SpellHitFn(classic_spell_pri_power_word_shield::HandleAfterHit);
    }
};

// 2652, 19261, 19262, 19264, 19265, 19266 - Touch of Weakness (Undead): "The next melee attack against the caster will cause X Shadow damage
// and reduce the attacker's melee attack power by Y for 2 min." The aura triggers 28598, a dummy; the attacker gets the rank's 2943, 19249,
// 19251, 19252, 19253, 19254 instead (VMaNGOS spell_priest.cpp). One charge (spell_proc).
class classic_spell_pri_touch_of_weakness : public AuraScript
{
    static uint32 GetRankSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 2652:  return 2943;
            case 19261: return 19249;
            case 19262: return 19251;
            case 19264: return 19252;
            case 19265: return 19253;
            case 19266: return 19254;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return GetRankSpell(spellInfo->Id) && ValidateSpellInfo({ GetRankSpell(spellInfo->Id) });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* attacker = eventInfo.GetActor();
        return attacker && attacker != GetTarget() && attacker->IsAlive();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActor(), GetRankSpell(GetId()), CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_touch_of_weakness::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_touch_of_weakness::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 18137, 19308, 19309, 19310, 19311, 19312 - Shadowguard (Troll): "When a spell, melee or ranged attack hits the caster, the attacker will
// be struck for X Shadow damage." The aura triggers 28376, a dummy; the attacker gets the rank's 28377-28382 instead (VMaNGOS
// UnitAuraProcHandler.cpp). Charges and the internal cooldown are the aura's own data.
class classic_spell_pri_shadowguard : public AuraScript
{
    static uint32 GetRankSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 18137: return 28377;
            case 19308: return 28378;
            case 19309: return 28379;
            case 19310: return 28380;
            case 19311: return 28381;
            case 19312: return 28382;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return GetRankSpell(spellInfo->Id) && ValidateSpellInfo({ GetRankSpell(spellInfo->Id) });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* attacker = eventInfo.GetActor();
        return attacker && attacker != GetTarget() && attacker->IsAlive();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActor(), GetRankSpell(GetId()), CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_shadowguard::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_shadowguard::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 20711 - Spirit of Redemption: "Upon death, the priest becomes the Spirit of Redemption for 15 sec. [...] When the effect ends, the priest
// dies." In 1.60 the talent is a dummy aura (the retail script hooks an absorb that does not exist). Damage that would kill a priest with the
// talent is prevented, and the priest gets 27827 (full health and the spirit form: 27792 / 27795, which kills him when it ends; VMaNGOS
// Unit.cpp). Not in a duel.
class classic_unit_spirit_of_redemption : public UnitScript
{
public:
    classic_unit_spirit_of_redemption() : UnitScript("classic_unit_spirit_of_redemption") { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        Player* priest = Object::ToPlayer(victim);
        if (!priest || !damage || damage < priest->GetHealth() || priest->GetClass() != CLASS_PRIEST || !priest->IsAlive())
            return;

        if (!priest->HasAura(SPELL_PRI_SPIRIT_OF_REDEMPTION) || priest->HasAura(SPELL_PRI_SPIRIT_OF_REDEMPTION_FORM))
            return;

        if (priest->duel && attacker && priest->duel->Opponent == attacker->GetControllingPlayer())
            return;

        damage = 0;
        priest->CastSpell(priest, SPELL_PRI_SPIRIT_OF_REDEMPTION_FORM, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, int32(priest->GetMaxHealth())));
        priest->SetFullHealth();
    }
};

/*######
## Paladin
######*/

// 20185, 20344, 20345, 20346 - Judgement of Light: "Melee attacks made against you have a chance of causing the attacker to be healed for X."
// 20186, 20354, 20355 - Judgement of Wisdom: "Attacks and spells used against the target have a chance of restoring X of the attacker's mana."
// The debuffs trigger 5373 / 1826 ("Intermediate", dummies); the attacker casts the rank's heal 20267, 20341-20343 or mana 20268, 20352,
// 20353 on himself instead, without the paladin as caster (no threat for him; VMaNGOS UnitAuraProcHandler.cpp). Proc chance and flags are
// the debuff's own data.
class classic_spell_pal_judgement_of_light_wisdom : public AuraScript
{
    static uint32 GetRankSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 20185: return 20267;
            case 20344: return 20341;
            case 20345: return 20342;
            case 20346: return 20343;
            case 20186: return 20268;
            case 20354: return 20352;
            case 20355: return 20353;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return GetRankSpell(spellInfo->Id) && ValidateSpellInfo({ GetRankSpell(spellInfo->Id) });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* attacker = eventInfo.GetActor();
        if (!attacker || attacker == GetTarget() || !attacker->IsAlive())
            return false;

        // Judgement of Wisdom: only attackers that use mana
        SpellInfo const* rankSpell = sSpellMgr->GetSpellInfo(GetRankSpell(GetId()), DIFFICULTY_NONE);
        if (rankSpell && rankSpell->HasEffect(SPELL_EFFECT_ENERGIZE))
            return attacker->GetMaxPower(POWER_MANA) > 0;
        return true;
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* attacker = eventInfo.GetActor();
        attacker->CastSpell(attacker, GetRankSpell(GetId()), CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pal_judgement_of_light_wisdom::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_judgement_of_light_wisdom::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 20425, 20961, 20962, 20967, 20968 - Judgement of Command (cast by Judgement for Seal of Command 20375, 20915, 20918-20920): a dummy whose
// points name the rank's damage spell 20467, 20964, 20963, 20965, 20966 (VMaNGOS spell_paladin.cpp)
class classic_spell_pal_judgement_of_command : public SpellScript
{
    static uint32 GetDamageSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 20425: return 20467;
            case 20961: return 20964;
            case 20962: return 20963;
            case 20967: return 20965;
            case 20968: return 20966;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return GetDamageSpell(spellInfo->Id) && ValidateSpellInfo({ GetDamageSpell(spellInfo->Id) });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, GetDamageSpell(GetSpellInfo()->Id), GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_judgement_of_command::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 20467, 20963, 20964, 20965, 20966 - Judgement of Command (damage): "instantly causing X/2 Holy damage, X if the target is stunned or
// incapacitated." The base damage is halved unless the target is stunned or incapacitated; spell power is added in full (VMaNGOS).
class classic_spell_pal_judgement_of_command_damage : public SpellScript
{
    static bool IsStunnedOrIncapacitated(Unit const* target)
    {
        return target->HasUnitState(UNIT_STATE_STUNNED)
            || target->HasAuraWithMechanic((UI64LIT(1) << MECHANIC_STUN) | (UI64LIT(1) << MECHANIC_KNOCKOUT) | (UI64LIT(1) << MECHANIC_SAPPED));
    }

    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& damage, int32& /*flatMod*/, float& /*pctMod*/)
    {
        if (victim && !IsStunnedOrIncapacitated(victim))
            damage /= 2;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_pal_judgement_of_command_damage::CalculateDamage);
    }
};

// 20210 - Illumination: "After getting a critical effect from your Flash of Light, Holy Light, Light's Vigil, or Holy Shock heal spell you
// have a X% chance to gain Mana equal to Y% of the base cost of the spell." Effect 0 (the chance) only triggers the "Dummy Trigger" 18350;
// effect 2 is the percentage; the energize is 20272 (VMaNGOS UnitAuraProcHandler.cpp). Without a trait amount on effect 0 every critical
// heal counts (vanilla rank 5). The Holy Shock and Light's Vigil heals are triggered, so the cost is that of the spell that was cast.
class classic_spell_pal_illumination : public AuraScript
{
    static constexpr uint32 SPELL_PAL_ILLUMINATION_ENERGIZE = 20272;

    static uint32 GetCastSpell(uint32 healSpellId)
    {
        switch (healSpellId)
        {
            case 25914:   return 20473;     // Holy Shock
            case 25913:   return 20929;
            case 25903:   return 20930;
            case 1311605: return 1311606;
            case 1310912: return 1310911;   // Light's Vigil
            case 1311591: return 1311590;
            case 1311596: return 1311595;
            default:      return healSpellId;
        }
    }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PAL_ILLUMINATION_ENERGIZE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        _mana = 0;
        HealInfo* healInfo = eventInfo.GetHealInfo();
        SpellInfo const* healSpell = eventInfo.GetSpellInfo();
        if (!healInfo || !healInfo->GetHeal() || !healSpell || !(eventInfo.GetHitMask() & PROC_HIT_CRITICAL)
            || !ClassicSpellNameIs(healSpell, { "Flash of Light", "Holy Light", "Light's Vigil", "Holy Shock" }))
            return false;

        AuraEffect const* chance = GetEffect(EFFECT_0);
        AuraEffect const* pct = GetEffect(EFFECT_2);
        if (!pct || (chance && chance->GetAmount() > 0 && !roll_chance(float(chance->GetAmount()))))
            return false;

        SpellInfo const* castSpell = sSpellMgr->GetSpellInfo(GetCastSpell(healSpell->Id), DIFFICULTY_NONE);
        if (castSpell)
            _mana = CalculatePct(GetClassicBaseManaCostMWPP(castSpell, GetTarget()), pct->GetAmountAsInt());
        return _mana > 0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_PAL_ILLUMINATION_ENERGIZE, CastSpellExtraArgs(aurEff).AddSpellMod(SPELLVALUE_BASE_POINT0, _mana));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pal_illumination::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_illumination::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }

    int32 _mana = 0;
};

void AddSC_classic_spells_mage_warlock_priest_paladin()
{
    RegisterSpellScript(classic_spell_mage_cold_snap);
    RegisterSpellScript(classic_spell_mage_master_of_elements);
    RegisterSpellScript(classic_spell_mage_combustion);
    RegisterSpellScript(classic_spell_mage_combustion_crit);
    RegisterSpellScript(classic_spell_warl_nightfall);
    RegisterSpellScript(classic_spell_warl_pyroclasm);
    RegisterSpellScript(classic_spell_warl_pyroclasm_rain_of_fire);
    RegisterSpellScript(classic_spell_pri_holy_nova);
    RegisterSpellScript(classic_spell_pri_power_word_shield);
    RegisterSpellScript(classic_spell_pri_touch_of_weakness);
    RegisterSpellScript(classic_spell_pri_shadowguard);
    new classic_unit_spirit_of_redemption();
    RegisterSpellScript(classic_spell_pal_judgement_of_light_wisdom);
    RegisterSpellScript(classic_spell_pal_judgement_of_command);
    RegisterSpellScript(classic_spell_pal_judgement_of_command_damage);
    RegisterSpellScript(classic_spell_pal_illumination);
}
