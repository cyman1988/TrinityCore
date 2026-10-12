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

// Classic 1.60 warrior, rogue and hunter abilities and talents whose client spell does its work in a dummy effect or dummy aura
// (Spell::EffectDummy and dummy procs do nothing for class spells), ported from VMaNGOS (SpellEffects.cpp, UnitAuraProcHandler.cpp,
// scripts/spells/spell_warrior.cpp, spell_rogue.cpp). Talent ranks come from TraitDefinitionEffectPoints curves (the rank value is the
// aura amount), so the scripts read the amounts and do not hardcode rank values.

#include "ScriptMgr.h"
#include "Pet.h"
#include "Player.h"
#include "Random.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

enum ClassicWarriorRogueHunterSpells
{
    // warrior
    SPELL_WARRIOR_EXECUTE_DAMAGE                = 20647,
    SPELL_WARRIOR_DEEP_WOUNDS_TALENT            = 12834,
    SPELL_WARRIOR_DEEP_WOUND_BLEED              = 412609,   // periodic dummy, every 3 sec
    SPELL_WARRIOR_DEEP_WOUND_DAMAGE             = 412613,   // the tick damage
    SPELL_WARRIOR_FLURRY_BUFF                   = 12966,    // 3 charges, melee haste
    SPELL_WARRIOR_SWEEPING_STRIKES_DAMAGE       = 12723,    // flat damage (base points), cannot crit
    SPELL_WARRIOR_SWEEPING_STRIKES_WEAPON       = 26654,    // normalized weapon damage
    SPELL_WARRIOR_RETALIATION_STRIKE            = 22858,
    SPELL_WARRIOR_LAST_STAND_HEALTH             = 12976,
    SPELL_WARRIOR_IMPROVED_BERSERKER_RAGE       = 20500,
    SPELL_WARRIOR_IMPROVED_BERSERKER_RAGE_RAGE  = 23690,    // energize rage
    SPELL_WARRIOR_WHIRLWIND                     = 1680,

    // rogue
    SPELL_ROGUE_BLADE_FLURRY_DAMAGE             = 1226883,  // flat damage (base points), cannot crit, ignores caster modifiers
    SPELL_ROGUE_RELENTLESS_STRIKES_EFFECT       = 14181,    // $s1 energy
    SPELL_ROGUE_PREPARATION                     = 14185,
    SPELL_ROGUE_REMORSELESS                     = 14143,

    // hunter
    SPELL_HUNTER_IMPROVED_MEND_PET              = 19572,
    SPELL_HUNTER_IMPROVED_MEND_PET_CLEANSE      = 24406,
};

// Damage of a hit before the victim's armor (VMaNGOS "reconstitute damage before armor reduction"), for the extra hits of Sweeping
// Strikes and Blade Flurry: their damage spells are physical, so the second target's own armor applies to them.
static uint32 ClassicDamageBeforeArmor(Unit* attacker, Unit* victim, uint32 damage)
{
    if (!damage || !victim)
        return damage;

    uint32 reducedHundred = Unit::CalcArmorReducedDamage(attacker, victim, 100, nullptr, BASE_ATTACK);
    if (!reducedHundred)
        return damage;

    return uint32(uint64(damage) * 100 / reducedHundred);
}

// Combo points a finishing move used: the spell's combo point cost (taken before the effects run), else the caster's points.
static int32 ClassicSpentComboPoints(Spell const* spell, Unit const* caster)
{
    if (spell)
        if (Optional<int32> cost = spell->GetPowerTypeCostAmount(POWER_COMBO_POINTS); cost && *cost > 0)
            return *cost;

    return caster ? caster->GetPower(POWER_COMBO_POINTS) : 0;
}

// SpellEffectInfo::CalcValue adds PointsPerResource x the caster's current combo points. When the combo point cost was already taken
// that is 0, so a finishing move would lose its per point damage: the missing part, with the points the spell used.
static int32 ClassicMissingComboDamage(SpellEffectInfo const& effect, Spell const* spell, Unit const* caster)
{
    if (!caster || effect.PointsPerResource == 0.0f || effect.Scaling.Coefficient != 0.0f)
        return 0;

    int32 missing = ClassicSpentComboPoints(spell, caster) - caster->GetPower(POWER_COMBO_POINTS);
    return missing > 0 ? int32(std::round(effect.PointsPerResource * missing)) : 0;
}

/*######
## Warrior
######*/

// 5308, 20658, 20660, 20661, 20662 - Execute: "Attempt to finish off a wounded foe, causing $s1 damage and converting each extra point
// of rage into $*10;F1 additional damage." Effect 0 is a dummy (base damage), its DmgMultiplier (ChainAmplitude: 0.3 ... 1.5 per stored
// rage unit, rage x10) is the per rage damage. Like VMaNGOS: 20647 deals base + rage x multiplier and all remaining rage is used.
class classic_spell_warr_execute : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_EXECUTE_DAMAGE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* warrior = GetCaster();
        Unit* target = GetHitUnit();
        if (!target || target == warrior)
            return;

        float perRage = GetEffectInfo().ChainAmplitude;
        int32 rage = warrior->GetPower(POWER_RAGE);
        int32 damage = GetEffectValueAsInt() + int32(std::round(float(rage) * perRage));
        warrior->SetPower(POWER_RAGE, 0);

        warrior->CastSpell(target, SPELL_WARRIOR_EXECUTE_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .SetTriggeringSpell(GetSpell())
            .AddSpellBP0(std::max(1, damage)));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_warr_execute::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 23881, 23892, 23893, 23894 - Bloodthirst: "Instantly attack the target causing damage equal to $s2% of your Attack Power plus $s1".
// Effect 1 (the attack power percent) is a dummy; it is added to the effect 0 damage.
class classic_spell_warr_bloodthirst : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void CalculateDamage(SpellEffectInfo const& spellEffectInfo, Unit* /*victim*/, int32& /*damage*/, int32& flatMod, float& /*pctMod*/)
    {
        if (spellEffectInfo.EffectIndex != EFFECT_0)
            return;

        Unit* warrior = GetCaster();
        float apPct = float(GetSpellInfo()->GetEffect(EFFECT_1).CalcValue(warrior));
        flatMod += int32(CalculatePct(warrior->GetTotalAttackPowerValue(BASE_ATTACK), apPct));
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_warr_bloodthirst::CalculateDamage);
    }
};

// 12834 - Deep Wounds: "Your critical strikes cause your opponent to Bleed, dealing $m1% of your melee weapon's average damage over
// $412609d." The talent triggers the dummy 12162; instead the bleed 412609 gets the per tick share of the rank percent of the average
// weapon damage (the weapon that crit, attack power included, like VMaNGOS). spell_proc: melee critical hits.
class classic_spell_warr_deep_wounds : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_DEEP_WOUND_BLEED, SPELL_WARRIOR_DEEP_WOUND_DAMAGE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        return target && target != GetTarget() && target->IsAlive() && eventInfo.GetDamageInfo();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        SpellInfo const* bleed = sSpellMgr->GetSpellInfo(SPELL_WARRIOR_DEEP_WOUND_BLEED, DIFFICULTY_NONE);
        if (aurEff->GetAmount() <= 0 || !bleed || !bleed->GetEffect(EFFECT_0).ApplyAuraPeriod)
            return;

        Unit* warrior = GetTarget();
        bool offHand = eventInfo.GetDamageInfo()->GetAttackType() == OFF_ATTACK && warrior->haveOffhandWeapon();
        float minDamage = offHand ? float(warrior->m_unitData->MinOffHandDamage) : float(warrior->m_unitData->MinDamage);
        float maxDamage = offHand ? float(warrior->m_unitData->MaxOffHandDamage) : float(warrior->m_unitData->MaxDamage);
        float total = CalculatePct((minDamage + maxDamage) / 2.0f, float(aurEff->GetAmount()));

        int32 ticks = std::max(1, bleed->GetMaxDuration() / int32(bleed->GetEffect(EFFECT_0).ApplyAuraPeriod));
        int32 perTick = std::max(1, int32(total / float(ticks)));
        warrior->CastSpell(eventInfo.GetActionTarget(), SPELL_WARRIOR_DEEP_WOUND_BLEED, CastSpellExtraArgs(aurEff).AddSpellBP0(perTick));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_deep_wounds::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_deep_wounds::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 412609 - Deep Wound: "Bleeding." Periodic dummy; every tick deals its amount with 412613 (bleed, ignores armor: spell_custom_attr).
class classic_spell_warr_deep_wound_bleed : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_DEEP_WOUND_DAMAGE });
    }

    void HandleTick(AuraEffect const* aurEff)
    {
        Unit* warrior = GetCaster();
        Unit* target = GetTarget();
        if (!warrior || !target->IsAlive() || aurEff->GetAmount() <= 0)
            return;

        warrior->CastSpell(target, SPELL_WARRIOR_DEEP_WOUND_DAMAGE, CastSpellExtraArgs(aurEff).AddSpellBP0(aurEff->GetAmount()));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_warr_deep_wound_bleed::HandleTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 12319 - Flurry (warrior): "Increases your melee attack speed by $m1% for your next $12966n swings after dealing a melee critical
// strike." 12966 (3 charges, used by swings) with the talent amount as its haste (as classic_spell_sha_flurry does for 16256/16257).
class classic_spell_warr_flurry : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_FLURRY_BUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmount() > 0)
            args.AddSpellBP0(aurEff->GetAmount());
        GetTarget()->CastSpell(GetTarget(), SPELL_WARRIOR_FLURRY_BUFF, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_flurry::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 12292 - Sweeping Strikes: "Your next $n melee attacks strike an additional nearby opponent." (5 charges, used only when a second
// target is hit). VMaNGOS: the damage of the hit (before armor) goes to a random other enemy in melee range (8 yd for Whirlwind) with
// 12723. Execute (20647): a second target below 20% takes the full Execute damage, any other one a normal weapon hit (26654).
class classic_spell_warr_sweeping_strikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_SWEEPING_STRIKES_DAMAGE, SPELL_WARRIOR_SWEEPING_STRIKES_WEAPON });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        _procTarget = nullptr;

        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        Unit* victim = eventInfo.GetActionTarget();
        // non damaging hits (Rend's initial hit, ...) do not trigger it
        if (!damageInfo || damageInfo->GetDamage() <= 1 || !victim)
            return false;

        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (spellInfo && (spellInfo->Id == SPELL_WARRIOR_SWEEPING_STRIKES_DAMAGE || spellInfo->Id == SPELL_WARRIOR_SWEEPING_STRIKES_WEAPON))
            return false;

        Unit* warrior = GetTarget();
        float radius = spellInfo && spellInfo->Id == SPELL_WARRIOR_WHIRLWIND ? 8.0f : NOMINAL_MELEE_RANGE;
        _procTarget = warrior->SelectNearbyTarget(victim, radius);
        return _procTarget && warrior->IsValidAttackTarget(_procTarget);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* warrior = GetTarget();
        Unit* victim = eventInfo.GetActionTarget();
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (spellInfo && spellInfo->Id == SPELL_WARRIOR_EXECUTE_DAMAGE && _procTarget->GetHealthPct() > 20.0f)
        {
            warrior->CastSpell(_procTarget, SPELL_WARRIOR_SWEEPING_STRIKES_WEAPON, CastSpellExtraArgs(aurEff));
            return;
        }

        uint32 damage = ClassicDamageBeforeArmor(warrior, victim, eventInfo.GetDamageInfo()->GetDamage());
        warrior->CastSpell(_procTarget, SPELL_WARRIOR_SWEEPING_STRIKES_DAMAGE, CastSpellExtraArgs(aurEff).AddSpellBP0(int32(damage)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_sweeping_strikes::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_sweeping_strikes::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    Unit* _procTarget = nullptr;
};

// 20230 - Retaliation: "Instantly counterattack any enemy that strikes you in melee for $d. Melee attacks made from behind cannot be
// counterattacked. A maximum of $n attacks will cause retaliation." 22858 strikes back; like VMaNGOS not while stunned, confused or
// fleeing (patch 1.7), and not against attacks from behind.
class classic_spell_warr_retaliation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_RETALIATION_STRIKE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* warrior = GetTarget();
        Unit* attacker = eventInfo.GetActor();
        if (!attacker || attacker == warrior || !attacker->IsAlive() || !warrior->IsValidAttackTarget(attacker))
            return false;

        // two warriors with Retaliation would strike each other forever
        if (SpellInfo const* spellInfo = eventInfo.GetSpellInfo(); spellInfo && spellInfo->Id == SPELL_WARRIOR_RETALIATION_STRIKE)
            return false;

        return !warrior->HasUnitState(UNIT_STATE_CONTROLLED) && warrior->isInFront(attacker);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActor(), SPELL_WARRIOR_RETALIATION_STRIKE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_retaliation::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_retaliation::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 12975 - Last Stand: "When activated, this ability temporarily grants you $12976s1% of your maximum health for $12976d. After the
// effect expires, the health is lost." 12976 (aura 34, flat health) gets that percent of the maximum health.
class classic_spell_warr_last_stand : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_WARRIOR_LAST_STAND_HEALTH, EFFECT_0 } });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* warrior = GetCaster();
        SpellInfo const* health = sSpellMgr->AssertSpellInfo(SPELL_WARRIOR_LAST_STAND_HEALTH, DIFFICULTY_NONE);
        float pct = float(health->GetEffect(EFFECT_0).CalcValue(warrior));
        int32 amount = int32(warrior->CountPctFromMaxHealth(pct));
        if (amount <= 0)
            return;

        warrior->CastSpell(warrior, SPELL_WARRIOR_LAST_STAND_HEALTH, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .SetTriggeringSpell(GetSpell())
            .AddSpellBP0(amount));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_warr_last_stand::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 18499 - Berserker Rage, with 20500 - Improved Berserker Rage: "Your Berserker Rage ability will instantly generate $/10;m1 Rage and has
// a $m2% chance to remove all movement impairing effects when activated." Both talent effects are dummies; 23690 energizes the rage.
class classic_spell_warr_berserker_rage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_IMPROVED_BERSERKER_RAGE_RAGE });
    }

    void HandleAfterCast()
    {
        Unit* warrior = GetCaster();
        AuraEffect const* rage = warrior->GetAuraEffect(SPELL_WARRIOR_IMPROVED_BERSERKER_RAGE, EFFECT_0);
        if (!rage)
            return;

        if (rage->GetAmountAsInt() > 0)
            warrior->CastSpell(warrior, SPELL_WARRIOR_IMPROVED_BERSERKER_RAGE_RAGE, CastSpellExtraArgs(rage).AddSpellBP0(rage->GetAmount()));

        if (AuraEffect const* snares = warrior->GetAuraEffect(SPELL_WARRIOR_IMPROVED_BERSERKER_RAGE, EFFECT_1))
            if (roll_chance(snares->GetAmountAsInt()))
                warrior->RemoveMovementImpairingAuras(true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_spell_warr_berserker_rage::HandleAfterCast);
    }
};

/*######
## Rogue
######*/

// 2098, 6760, 6761, 6762, 8623, 8624, 11299, 11300 - Eviscerate: "Finishing move that causes damage per combo point, increased by Attack
// Power". Effect 1 is a dummy and the damage effect has no attack power coefficient; VMaNGOS (patch 1.12): + attack power x 3% per
// combo point. Also restores the per point damage when the combo point cost was taken before the damage was calculated.
class classic_spell_rog_eviscerate : public SpellScript
{
    void CalculateDamage(SpellEffectInfo const& spellEffectInfo, Unit* /*victim*/, int32& /*damage*/, int32& flatMod, float& /*pctMod*/)
    {
        if (spellEffectInfo.EffectIndex != EFFECT_0)
            return;

        Unit* rogue = GetCaster();
        int32 comboPoints = ClassicSpentComboPoints(GetSpell(), rogue);
        if (comboPoints <= 0)
            return;

        flatMod += ClassicMissingComboDamage(spellEffectInfo, GetSpell(), rogue);
        flatMod += int32(rogue->GetTotalAttackPowerValue(BASE_ATTACK) * 0.03f * float(comboPoints));
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_rog_eviscerate::CalculateDamage);
    }
};

// 1943, 8639, 8640, 11273, 11274, 11275 - Rupture: "Finishing move that causes damage over time, increased by your Attack Power. Lasts
// longer per combo point". Effects 1 and 2 are dummies. VMaNGOS (patch 1.12): every tick + attack power x 1% per combo point, at most
// 3% (4/10/18/21/24% in total for 1-5 points). Also restores the per point tick damage like Eviscerate.
class classic_spell_rog_rupture : public SpellScript
{
    void HandleAfterHit()
    {
        Aura* rupture = GetHitAura();
        AuraEffect* periodic = rupture ? rupture->GetEffect(EFFECT_0) : nullptr;
        Unit* rogue = GetCaster();
        if (!periodic || periodic->GetAuraType() != SPELL_AURA_PERIODIC_DAMAGE || periodic->GetCasterGUID() != rogue->GetGUID())
            return;

        int32 comboPoints = ClassicSpentComboPoints(GetSpell(), rogue);
        if (comboPoints <= 0)
            return;

        int32 bonus = ClassicMissingComboDamage(GetSpellInfo()->GetEffect(EFFECT_0), GetSpell(), rogue);
        bonus += int32(rogue->GetTotalAttackPowerValue(BASE_ATTACK) * float(std::min(comboPoints, 3)) / 100.0f);
        if (bonus > 0)
            periodic->SetAmount(periodic->GetAmount() + bonus);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(classic_spell_rog_rupture::HandleAfterHit);
    }
};

// 13877 - Blade Flurry: "Increases your melee attack speed by $s1% and your melee attacks strike an additional nearby opponent." The
// client aura is only the haste; VMaNGOS: the damage of each hit (before armor) goes to a random other enemy within 5 yd, here with the
// client's Blade Flurry damage spell 1226883 (the retail 22482 is not in the client).
class classic_spell_rog_blade_flurry : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_BLADE_FLURRY_DAMAGE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        _procTarget = nullptr;

        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        Unit* victim = eventInfo.GetActionTarget();
        if (!damageInfo || !damageInfo->GetDamage() || !victim)
            return false;

        if (SpellInfo const* spellInfo = eventInfo.GetSpellInfo(); spellInfo && spellInfo->Id == SPELL_ROGUE_BLADE_FLURRY_DAMAGE)
            return false;

        Unit* rogue = GetTarget();
        _procTarget = rogue->SelectNearbyTarget(victim, NOMINAL_MELEE_RANGE);
        return _procTarget && rogue->IsValidAttackTarget(_procTarget);
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* rogue = GetTarget();
        uint32 damage = ClassicDamageBeforeArmor(rogue, eventInfo.GetActionTarget(), eventInfo.GetDamageInfo()->GetDamage());
        rogue->CastSpell(_procTarget, SPELL_ROGUE_BLADE_FLURRY_DAMAGE, CastSpellExtraArgs(GetEffect(EFFECT_0)).AddSpellBP0(int32(damage)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_blade_flurry::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_blade_flurry::HandleProc);
    }

    Unit* _procTarget = nullptr;
};

// 14179 - Relentless Strikes: "Your finishing moves have a $m1% chance per Combo Point to restore $14181s1 Energy." The client aura is
// an add target trigger without a spell mask (never triggers) of the dummy 14181. spell_proc: rogue spells on cast; the script checks the
// spell used combo points and rolls the chance per point (VMaNGOS), then gives the energy of 14181.
class classic_spell_rog_relentless_strikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_ROGUE_RELENTLESS_STRIKES_EFFECT, EFFECT_0 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* spell = eventInfo.GetProcSpell();
        AuraEffect const* talent = GetEffect(EFFECT_0);
        if (!spell || !talent || talent->GetAmountAsInt() <= 0 || spell->GetSpellInfo()->SpellFamilyName != SPELLFAMILY_ROGUE)
            return false;

        Optional<int32> comboPoints = spell->GetPowerTypeCostAmount(POWER_COMBO_POINTS);
        if (!comboPoints || *comboPoints <= 0)
            return false;

        return roll_chance(talent->GetAmountAsInt() * *comboPoints);
    }

    void HandleProc(ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        Unit* rogue = GetTarget();
        SpellInfo const* effect = sSpellMgr->AssertSpellInfo(SPELL_ROGUE_RELENTLESS_STRIKES_EFFECT, DIFFICULTY_NONE);
        int32 energy = effect->GetEffect(EFFECT_0).CalcValueAsInt(rogue);
        if (energy > 0)
            rogue->EnergizeBySpell(rogue, effect, energy, POWER_ENERGY);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_relentless_strikes::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_relentless_strikes::HandleProc);
    }
};

// 14185 - Preparation: "When activated, this ability immediately finishes the cooldown on your other Rogue abilities." (VMaNGOS: every
// rogue spell with a cooldown)
class classic_spell_rog_preparation : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* rogue = GetCaster();
        rogue->GetSpellHistory()->ResetCooldowns([](SpellHistory::CooldownEntry const& cooldown) -> bool
        {
            if (cooldown.SpellId == SPELL_ROGUE_PREPARATION)
                return false;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(cooldown.SpellId, DIFFICULTY_NONE);
            return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_ROGUE
                && (spellInfo->RecoveryTime > 0 || spellInfo->CategoryRecoveryTime > 0);
        }, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_rog_preparation::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 14144 - Remorseless Attacks: "After killing a non-trivial enemy, gives you a $m1% increased critical strike chance on your next
// Sinister Strike, Backstab, Ambush, Mutilate, or Ghostly Strike. Lasts $14143d." spell_proc: kills that give experience or honor;
// 14143 gets the talent amount.
class classic_spell_rog_remorseless_attacks : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_REMORSELESS });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        if (aurEff->GetAmount() <= 0)
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_REMORSELESS, CastSpellExtraArgs(aurEff).AddSpellBP0(aurEff->GetAmount()));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_rog_remorseless_attacks::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

/*######
## Hunter
######*/

// 136, 3111, 3661, 3662, 13542, 13543, 13544 - Mend Pet, with 19572 - Improved Mend Pet: "Gives your Mend Pet spell a $s1% chance of
// cleansing $24406s1 Curse, Disease, Magic, or Poison effect from your pet each time it heals". The talent chance is a dummy; VMaNGOS
// (override class script 4086/4087) rolls it on every heal tick and casts 24406 on the pet.
class classic_spell_hun_mend_pet : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_IMPROVED_MEND_PET_CLEANSE });
    }

    void HandleTick(AuraEffect const* aurEff)
    {
        Unit* hunter = GetCaster();
        Unit* pet = GetTarget();
        if (!hunter || !pet->IsAlive() || pet == hunter)
            return;

        AuraEffect const* talent = hunter->GetAuraEffect(SPELL_HUNTER_IMPROVED_MEND_PET, EFFECT_0);
        if (!talent || !roll_chance(talent->GetAmountAsInt()))
            return;

        hunter->CastSpell(pet, SPELL_HUNTER_IMPROVED_MEND_PET_CLEANSE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_hun_mend_pet::HandleTick, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
    }
};

void AddSC_classic_spells_warrior_rogue_hunter()
{
    // warrior
    RegisterSpellScript(classic_spell_warr_execute);
    RegisterSpellScript(classic_spell_warr_bloodthirst);
    RegisterSpellScript(classic_spell_warr_deep_wounds);
    RegisterSpellScript(classic_spell_warr_deep_wound_bleed);
    RegisterSpellScript(classic_spell_warr_flurry);
    RegisterSpellScript(classic_spell_warr_sweeping_strikes);
    RegisterSpellScript(classic_spell_warr_retaliation);
    RegisterSpellScript(classic_spell_warr_last_stand);
    RegisterSpellScript(classic_spell_warr_berserker_rage);

    // rogue
    RegisterSpellScript(classic_spell_rog_eviscerate);
    RegisterSpellScript(classic_spell_rog_rupture);
    RegisterSpellScript(classic_spell_rog_blade_flurry);
    RegisterSpellScript(classic_spell_rog_relentless_strikes);
    RegisterSpellScript(classic_spell_rog_preparation);
    RegisterSpellScript(classic_spell_rog_remorseless_attacks);

    // hunter
    RegisterSpellScript(classic_spell_hun_mend_pet);
}
