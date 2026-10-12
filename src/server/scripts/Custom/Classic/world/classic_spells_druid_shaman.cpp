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

// Classic 1.60 druid and shaman spell scripts: class mechanics that the 1.60 client data leaves to the server (dummy effects,
// proc auras whose trigger spell is a dummy).

#include "ScriptMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

enum ClassicDruidShamanSpells
{
    // Druid
    SPELL_CDS_LEADER_OF_THE_PACK            = 17007,    // talent, passive dummy
    SPELL_CDS_LEADER_OF_THE_PACK_AURA       = 24932,    // party area aura: melee/ranged critical strike chance
    SPELL_CDS_HEART_OF_THE_WILD             = 17003,    // talent: effect 1 Bear stamina %, effect 2 Cat strength % (dummies)
    SPELL_CDS_HEART_OF_THE_WILD_BEAR        = 24899,    // SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, stamina
    SPELL_CDS_HEART_OF_THE_WILD_CAT         = 24900,    // SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, strength
    SPELL_CDS_PREDATORY_STRIKES             = 16972,
    SPELL_CDS_FRENZIED_REGENERATION_HEAL    = 22845,
    SPELL_CDS_ENRAGE_ARMOR                  = 25503,    // effect 1: SPELL_AURA_MOD_BASE_RESISTANCE_PCT (armor)
    SPELL_CDS_ENRAGE_NO_ARMOR_PENALTY       = 456328,   // rune: "it no longer reduces your armor"

    // Shaman
    SPELL_CDS_FLAMETONGUE_ATTACK            = 10444,
};

/*######
## Druid
######*/

// Combo points used by a finisher. The 1.60 finishers pay their combo points as a spell power cost, which Spell::TakePower takes
// before the effects are calculated, so SpellEffectInfo::CalcValue (EffectPointsPerResource x current combo points) no longer
// sees them; the spent amount is in the spell's power costs.
static int32 GetFinisherComboPoints(Unit const* caster, Spell const* spell)
{
    if (spell)
        if (Optional<int32> spent = spell->GetPowerTypeCostAmount(POWER_COMBO_POINTS))
            return *spent;
    return caster->GetPower(POWER_COMBO_POINTS);
}

// The per combo point part of an effect that CalcValue did not add (it adds PointsPerResource x the combo points still there).
static double GetMissingComboPointValue(Unit const* caster, SpellEffectInfo const& effectInfo, int32 comboPoints)
{
    return effectInfo.PointsPerResource * double(comboPoints - caster->GetPower(POWER_COMBO_POINTS));
}

// 22568, 22827, 22828, 22829, 31018 - Ferocious Bite: "Finishing move that causes damage per combo point and converts each extra
// point of energy into ${$m2/100}.1 additional damage. Damage is increased by your Attack Power." Effect 1 is a dummy (bp = damage
// per energy x 100). Vanilla 1.12 (VMaNGOS spell_druid.cpp): + 3% attack power per combo point, all remaining energy is used.
class classic_spell_dru_ferocious_bite : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void CalculateDamage(SpellEffectInfo const& effectInfo, Unit* /*victim*/, int32& damage, int32& /*flatMod*/, float& /*pctMod*/)
    {
        Unit* caster = GetCaster();
        int32 comboPoints = GetFinisherComboPoints(caster, GetSpell());

        double bonus = GetMissingComboPointValue(caster, effectInfo, comboPoints);
        bonus += caster->GetTotalAttackPowerValue(BASE_ATTACK) * 0.03 * comboPoints;

        // each extra point of energy (the cost is already paid)
        int32 energy = caster->GetPower(POWER_ENERGY);
        if (energy > 0)
        {
            bonus += energy * GetEffectInfo(EFFECT_1).CalcValue(caster) / 100.0;
            caster->SetPower(POWER_ENERGY, 0);
        }

        damage = std::max(0, damage + int32(bonus));
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_dru_ferocious_bite::CalculateDamage);
    }
};

// 1079, 9492, 9493, 9752, 9894, 9896 - Rip: "Finishing move that causes damage over time. Damage increases per combo point and by your
// Attack Power" (per tick: $m1 + $b1 per combo point). Effect 1 is a dummy. Vanilla 1.12 (VMaNGOS Aura::CalculateDotDamage): + attack
// power x 1% per combo point per tick, at most 4 combo points.
class classic_spell_dru_rip : public AuraScript
{
    void CalculateAmount(AuraEffect const* aurEff, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        // the bonus is fixed when Rip is applied (it depends on the combo points spent)
        canBeRecalculated = false;

        Unit* caster = GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        int32 comboPoints = GetFinisherComboPoints(caster, caster->FindCurrentSpellBySpellId(GetId()));
        amount += GetMissingComboPointValue(caster, aurEff->GetSpellEffectInfo(), comboPoints);
        amount += caster->GetTotalAttackPowerValue(BASE_ATTACK) * std::min(comboPoints, 4) / 100.0;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(classic_spell_dru_rip::CalculateAmount, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 22842 - Frenzied Regeneration: "Converts up to 10 Rage per second into health for $d. Each point of Rage is converted into $s1%
// health." A periodic trigger aura without trigger spell; each tick takes up to 10 rage and heals with 22845 (VMaNGOS
// SpellAuras.cpp, Aura::TriggerSpell).
class classic_spell_dru_frenzied_regeneration : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CDS_FRENZIED_REGENERATION_HEAL });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        Unit* druid = GetTarget();
        int32 rage = std::min(druid->GetPower(POWER_RAGE), 100);    // rage is stored x10
        if (rage <= 0)
            return;

        druid->ModifyPower(POWER_RAGE, -rage);
        double heal = double(druid->GetMaxHealth()) * (rage / 10.0) * aurEff->GetAmount() / 100.0;
        if (heal >= 1.0)
            druid->CastSpell(druid, SPELL_CDS_FRENZIED_REGENERATION_HEAL, CastSpellExtraArgs(aurEff).AddSpellBP0(std::round(heal)));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_dru_frenzied_regeneration::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 18562 - Swiftmend: "Instantly heals a target with an active Rejuvenation or Regrowth effect for an amount equal to the full
// duration of the periodic effect of one of those spells." The heal effect has 1 point; the shortest Rejuvenation or Regrowth on
// the target is consumed (VMaNGOS spell_druid.cpp).
class classic_spell_dru_swiftmend : public SpellScript
{
    static bool IsRejuvenationOrRegrowth(uint32 spellId)
    {
        switch (spellId)
        {
            // Rejuvenation
            case 774: case 1058: case 1430: case 2090: case 2091: case 3627: case 8910: case 9839: case 9840: case 9841: case 25299:
            // Regrowth
            case 8936: case 8938: case 8939: case 8940: case 8941: case 9750: case 9856: case 9857: case 9858:
                return true;
            default:
                return false;
        }
    }

    static AuraEffect* FindHot(Unit* target)
    {
        AuraEffect* hot = nullptr;
        for (AuraEffect* aurEff : target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_HEAL))
            if (IsRejuvenationOrRegrowth(aurEff->GetId()))
                if (!hot || aurEff->GetBase()->GetDuration() < hot->GetBase()->GetDuration())
                    hot = aurEff;
        return hot;
    }

    SpellCastResult CheckCast()
    {
        Unit* target = GetExplTargetUnit();
        if (!target || !FindHot(target))
            return SPELL_FAILED_TARGET_AURASTATE;
        return SPELL_CAST_OK;
    }

    void CalculateHealing(SpellEffectInfo const& /*effectInfo*/, Unit* victim, int32& healing, int32& flatMod, float& pctMod)
    {
        AuraEffect* hot = FindHot(victim);
        if (!hot)
            return;

        // the periodic effect's tick with the healing bonus it had when cast, for all its ticks
        int32 tick = int32(hot->GetEstimatedAmount().value_or(hot->GetAmount()));
        healing = tick * int32(std::max<uint32>(hot->GetTotalTicks(), 1));
        flatMod = 0;
        pctMod = 1.0f;

        _consumedAura = hot->GetBase();
    }

    void ConsumeHot()
    {
        if (_consumedAura && !_consumedAura->IsRemoved())
            _consumedAura->Remove();
        _consumedAura = nullptr;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_dru_swiftmend::CheckCast);
        CalcHealing += SpellCalcHealingFn(classic_spell_dru_swiftmend::CalculateHealing);
        AfterHit += SpellHitFn(classic_spell_dru_swiftmend::ConsumeHot);
    }

    Aura* _consumedAura = nullptr;
};

// 768 - Cat Form, 5487 - Bear Form, 9634 - Dire Bear Form: form bonuses of druid talents (VMaNGOS Aura::HandleShapeshiftBoosts).
// 17007 - Leader of the Pack: "While in Cat Form, Bear Form, or Dire Bear Form, the Leader of the Pack increases the critical strike
// chance of all party members within $24932a1 yards by $24932s1%" (24932: party area aura).
// 17003 - Heart of the Wild: "while in Bear Form or Dire Bear Form your Stamina is increased by $s2% and while in Cat Form your
// Strength is increased by $s3%" (effects 1 and 2 are dummies; 24899 / 24900 carry the stat percentage).
class classic_spell_dru_feral_form_talents : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CDS_LEADER_OF_THE_PACK_AURA, SPELL_CDS_HEART_OF_THE_WILD_BEAR, SPELL_CDS_HEART_OF_THE_WILD_CAT });
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* druid = GetTarget();
        ShapeshiftForm const form = ShapeshiftForm(aurEff->GetMiscValue());
        if (form != FORM_CAT_FORM && form != FORM_BEAR_FORM && form != FORM_DIRE_BEAR_FORM)
            return;

        if (druid->HasAura(SPELL_CDS_LEADER_OF_THE_PACK))
            druid->CastSpell(druid, SPELL_CDS_LEADER_OF_THE_PACK_AURA, CastSpellExtraArgs(aurEff));

        if (Aura const* heartOfTheWild = druid->GetAura(SPELL_CDS_HEART_OF_THE_WILD))
        {
            SpellEffIndex const effIndex = form == FORM_CAT_FORM ? EFFECT_2 : EFFECT_1;
            if (AuraEffect const* bonus = heartOfTheWild->GetEffect(effIndex); bonus && bonus->GetAmount() > 0)
                druid->CastSpell(druid, form == FORM_CAT_FORM ? SPELL_CDS_HEART_OF_THE_WILD_CAT : SPELL_CDS_HEART_OF_THE_WILD_BEAR,
                    CastSpellExtraArgs(aurEff).AddSpellBP0(bonus->GetAmount()));
        }
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* druid = GetTarget();
        druid->RemoveAurasDueToSpell(SPELL_CDS_LEADER_OF_THE_PACK_AURA, druid->GetGUID());
        druid->RemoveAurasDueToSpell(SPELL_CDS_HEART_OF_THE_WILD_BEAR, druid->GetGUID());
        druid->RemoveAurasDueToSpell(SPELL_CDS_HEART_OF_THE_WILD_CAT, druid->GetGUID());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_dru_feral_form_talents::HandleApply, EFFECT_0, SPELL_AURA_MOD_SHAPESHIFT, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_dru_feral_form_talents::HandleRemove, EFFECT_0, SPELL_AURA_MOD_SHAPESHIFT, AURA_EFFECT_HANDLE_REAL);
    }
};

// 16972 - Predatory Strikes: "Increases your melee Attack Power in Cat Form, Bear Form, and Dire Bear Form by $s1% of your level."
// The attack power is added in Player::UpdateAttackPowerAndDamage; this updates it when the talent is learned or unlearned.
class classic_spell_dru_predatory_strikes : public AuraScript
{
    void HandleApplyOrRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* druid = GetTarget()->ToPlayer())
            druid->UpdateAttackPowerAndDamage();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_dru_predatory_strikes::HandleApplyOrRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_dru_predatory_strikes::HandleApplyOrRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 5229 - Enrage: "Instantly generates ${$m2/10} Rage and another $m1 Rage over $d, but reduces base armor by 27% in Bear Form and 16% in
// Dire Bear Form." Effect 3 is a dummy; the armor reduction is 25503 effect 1 (VMaNGOS spell_druid.cpp). The rune 456328 removes it.
class classic_spell_dru_enrage : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_CDS_ENRAGE_ARMOR, EFFECT_1 } });
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* druid = GetTarget();
        if (druid->HasAura(SPELL_CDS_ENRAGE_NO_ARMOR_PENALTY))
            return;

        int32 reduction;
        switch (druid->GetShapeshiftForm())
        {
            case FORM_BEAR_FORM:      reduction = -27; break;
            case FORM_DIRE_BEAR_FORM: reduction = -16; break;
            default: return;
        }

        druid->CastSpell(druid, SPELL_CDS_ENRAGE_ARMOR, CastSpellExtraArgs(aurEff).AddSpellMod(SPELLVALUE_BASE_POINT1, reduction));
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_CDS_ENRAGE_ARMOR, GetTarget()->GetGUID());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_dru_enrage::HandleApply, EFFECT_3, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_dru_enrage::HandleRemove, EFFECT_3, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

/*######
## Shaman
######*/

// 436519 - Flametongue Weapon (1.60 imbue aura). Every Flametongue enchant (5, 4, 3, 523, 1665, 1666, 7567) equips this dummy aura on
// the imbued weapon: "Each hit causes ${$8026m1/77-1} to ${$8026m1/25} additional Fire damage, based on the speed of the weapon.
// Slower weapons cause more fire damage per swing." The rank's proc spell (8026, 8028, 8029, 10445, 16343, 16344) is a dummy whose
// points are the damage per 100 sec of weapon speed; the damage is done by 10444 (VMaNGOS spell_shaman.cpp; 10444 carries its own
// spell power coefficient in the 1.60 data). Every melee hit with the imbued weapon (spell_proc).
class classic_spell_sha_flametongue_weapon : public AuraScript
{
    static uint32 GetProcSpell(uint32 enchantId)
    {
        switch (enchantId)
        {
            case 5:    return 8026;  // Rank 1
            case 4:    return 8028;  // Rank 2
            case 3:    return 8029;  // Rank 3
            case 523:  return 10445; // Rank 4
            case 1665: return 16343; // Rank 5
            case 1666:
            case 7567: return 16344; // Rank 6
            default:   return 0;
        }
    }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CDS_FLAMETONGUE_ATTACK, 8026, 8028, 8029, 10445, 16343, 16344 });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        _damage = 0;
        Player* player = GetTarget()->ToPlayer();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        Unit* victim = eventInfo.GetActionTarget();
        if (!player || !damageInfo || !victim || victim == player || damageInfo->GetAttackType() == RANGED_ATTACK)
            return false;

        // only hits made with the imbued weapon
        Item* weapon = player->GetWeaponForAttack(damageInfo->GetAttackType(), true);
        if (!weapon || weapon->GetGUID() != GetAura()->GetCastItemGUID())
            return false;

        uint32 procSpellId = GetProcSpell(weapon->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT));
        SpellInfo const* procSpell = procSpellId ? sSpellMgr->GetSpellInfo(procSpellId, DIFFICULTY_NONE) : nullptr;
        if (!procSpell)
            return false;

        double weaponSpeed = weapon->GetTemplate()->GetDelay() / 1000.0;
        _damage = procSpell->GetEffect(EFFECT_0).CalcValue(player) * 0.01 * weaponSpeed;
        return _damage >= 1.0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_CDS_FLAMETONGUE_ATTACK, CastSpellExtraArgs(aurEff).AddSpellBP0(std::round(_damage)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_flametongue_weapon::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_flametongue_weapon::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    double _damage = 0.0;
};

// 324, 325, 905, 945, 8134, 10431, 10432 - Lightning Shield: "When a spell, melee or ranged attack hits the caster, the attacker will be
// struck for $26364s1 Nature damage. This expends one lightning ball." The aura procs 26545, a dummy that does nothing; the rank's
// damage spell (26364 ... 26363) is cast on the attacker instead (VMaNGOS UnitAuraProcHandler.cpp). Charges and the 3.5 sec proc
// cooldown come from the client data.
class classic_spell_sha_lightning_shield : public AuraScript
{
    static uint32 GetDamageSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 324:   return 26364; // Rank 1
            case 325:   return 26365; // Rank 2
            case 905:   return 26366; // Rank 3
            case 945:   return 26367; // Rank 4
            case 8134:  return 26369; // Rank 5
            case 10431: return 26370; // Rank 6
            case 10432: return 26363; // Rank 7
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        uint32 damageSpell = GetDamageSpell(spellInfo->Id);
        return damageSpell && ValidateSpellInfo({ damageSpell });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* attacker = eventInfo.GetActor();
        return attacker && attacker != GetTarget() && attacker->IsAlive();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActor(), GetDamageSpell(GetId()), CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_lightning_shield::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_lightning_shield::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

void AddSC_classic_spells_druid_shaman()
{
    RegisterSpellScript(classic_spell_dru_ferocious_bite);
    RegisterSpellScript(classic_spell_dru_rip);
    RegisterSpellScript(classic_spell_dru_frenzied_regeneration);
    RegisterSpellScript(classic_spell_dru_swiftmend);
    RegisterSpellScript(classic_spell_dru_feral_form_talents);
    RegisterSpellScript(classic_spell_dru_predatory_strikes);
    RegisterSpellScript(classic_spell_dru_enrage);
    RegisterSpellScript(classic_spell_sha_flametongue_weapon);
    RegisterSpellScript(classic_spell_sha_lightning_shield);
}
