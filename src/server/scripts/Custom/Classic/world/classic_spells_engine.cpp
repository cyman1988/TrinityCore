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

// Classic 1.60 cross-class spell scripts: generic proc helpers and reactive abilities.

#include "ScriptMgr.h"
#include "Item.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

/*######
## classic_spell_proc_trigger_bp
######*/

// Generic: proc auras (PROC_TRIGGER_SPELL or DUMMY) whose 1.60 data has TriggerSpell 0 and the spell to cast in EffectBasePoints.
// The default proc handler returns early without a trigger spell (the proc charge is still used up). On proc the aura holder casts
// the base points spell: on itself when that spell needs no unit target, otherwise on the proc target. A melee proc of a player
// passes the weapon that hit as cast item (weapon speed based procs need it).
// Names:
//   classic_spell_proc_trigger_bp            - any proc allowed by spell_proc / the aura options
//   classic_spell_proc_trigger_bp_main_hand  - only main hand melee hits ("Main hand attacks ...")
// Bound here: Windfury Totem passives 8515 / 10609 / 10612 ("Main hand attacks have a $h% chance to grant $8516s2 extra attack with
// $8516m1 increased attack power", bp 8516 / 10608 / 10610) and Flametongue Totem passives 8230 / 10521 / 15036 ("Main hand attacks
// deal an additional ${$8253m1/77} to ${$8253M1/25} Fire damage", bp 8253 / 10523 / 16389). Flametongue Totem rank 2 passive 8250
// has bp 0 in the data; its rank proc 8248 is passed as fallback (classic_spell_sha_flametongue_totem_r2).
class classic_spell_proc_trigger_bp : public AuraScript
{
public:
    explicit classic_spell_proc_trigger_bp(bool mainHandOnly = false, uint32 fallbackSpellId = 0)
        : _mainHandOnly(mainHandOnly), _fallbackSpellId(fallbackSpellId) { }

private:
    uint32 GetProcSpellId(SpellEffectInfo const& effect) const
    {
        if (effect.TriggerSpell)
            return 0;   // the default handler casts it

        uint32 spellId = effect.BasePoints > 0.0f ? uint32(effect.BasePoints) : 0;
        if (spellId && sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
            return spellId;

        return _fallbackSpellId;
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        if (_fallbackSpellId && !ValidateSpellInfo({ _fallbackSpellId }))
            return false;

        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            if ((effect.IsAura(SPELL_AURA_PROC_TRIGGER_SPELL) || effect.IsAura(SPELL_AURA_DUMMY)) && GetProcSpellId(effect))
                return true;

        return false;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!_mainHandOnly)
            return true;

        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        return damageInfo && damageInfo->GetAttackType() == BASE_ATTACK && eventInfo.GetTypeMask() & MELEE_BASED_TRIGGER_MASK;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        if (aurEff->GetAuraType() != SPELL_AURA_PROC_TRIGGER_SPELL && aurEff->GetAuraType() != SPELL_AURA_DUMMY)
            return;

        uint32 spellId = GetProcSpellId(aurEff->GetSpellEffectInfo());
        SpellInfo const* spellInfo = spellId ? sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE) : nullptr;
        if (!spellInfo)
            return;

        PreventDefaultAction();

        Unit* caster = GetTarget();
        Unit* procTarget = eventInfo.GetActor() == caster ? eventInfo.GetActionTarget() : eventInfo.GetActor();
        Unit* target = spellInfo->NeedsExplicitUnitTarget() ? procTarget : caster;
        if (!target)
            return;

        CastSpellExtraArgs args(aurEff);
        if (Player* player = caster->ToPlayer())
            if (DamageInfo* damageInfo = eventInfo.GetDamageInfo(); damageInfo && damageInfo->GetAttackType() != RANGED_ATTACK
                && eventInfo.GetTypeMask() & MELEE_BASED_TRIGGER_MASK)
                if (Item* weapon = player->GetWeaponForAttack(damageInfo->GetAttackType(), true))
                    args.SetCastItem(weapon);

        caster->CastSpell(target, spellId, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_proc_trigger_bp::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_proc_trigger_bp::HandleProc, EFFECT_ALL, SPELL_AURA_ANY);
    }

    bool _mainHandOnly;
    uint32 _fallbackSpellId;
};

/*######
## classic_spell_warr_overpower
######*/

// 7384, 7887, 11584, 11585 - Overpower: "Instantly overpower the enemy, causing weapon damage plus $s1. Only useable after the target
// dodges." Like vanilla (VMaNGOS Unit.cpp REACTIVE_OVERPOWER) a dodge of the warrior's attack gives a combo point on that target for
// the reactive window (Unit::ProcSkillsAndReactives); the cast needs it on the current target and uses it up.
class classic_spell_warr_overpower : public SpellScript
{
    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!target || !caster->HasReactiveTimer(REACTIVE_OVERPOWER) || caster->GetClassicComboTarget() != target->GetGUID())
            return SPELL_FAILED_CASTER_AURASTATE;

        return SPELL_CAST_OK;
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        caster->ClearReactiveTimer(REACTIVE_OVERPOWER);
        caster->SetPower(POWER_COMBO_POINTS, 0);
        caster->SetClassicComboTarget(ObjectGuid::Empty);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_warr_overpower::CheckCast);
        AfterCast += SpellCastFn(classic_spell_warr_overpower::HandleAfterCast);
    }
};

void AddSC_classic_spells_engine()
{
    RegisterSpellScript(classic_spell_proc_trigger_bp);
    RegisterSpellScriptWithArgs(classic_spell_proc_trigger_bp, "classic_spell_proc_trigger_bp_main_hand", true);
    RegisterSpellScriptWithArgs(classic_spell_proc_trigger_bp, "classic_spell_sha_flametongue_totem_r2", true, 8248u);
    RegisterSpellScript(classic_spell_warr_overpower);
}
