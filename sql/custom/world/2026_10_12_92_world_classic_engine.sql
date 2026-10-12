-- Classic 1.60 cross-class / engine fixes (scripts in src/server/scripts/Custom/Classic/world/classic_spells_engine.cpp)

-- Generic "cast the spell in EffectBasePoints on proc" (classic_spell_proc_trigger_bp*): the 1.60 totem passives have TriggerSpell 0.
-- Windfury Totem 8515 / 10609 / 10612 (bp 8516 / 10608 / 10610), Flametongue Totem 8230 / 10521 / 15036 (bp 8253 / 10523 / 16389),
-- Flametongue Totem rank 2 8250 has bp 0 and uses its rank proc 8248. Delete by spell so other files can bind more spells to the
-- generic names.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (8515,10609,10612,8230,8250,10521,15036)
    AND `ScriptName` IN ('classic_spell_proc_trigger_bp','classic_spell_proc_trigger_bp_main_hand','classic_spell_sha_flametongue_totem_r2');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(8515,'classic_spell_proc_trigger_bp_main_hand'),
(10609,'classic_spell_proc_trigger_bp_main_hand'),
(10612,'classic_spell_proc_trigger_bp_main_hand'),
(8230,'classic_spell_proc_trigger_bp_main_hand'),
(8250,'classic_spell_sha_flametongue_totem_r2'),
(10521,'classic_spell_proc_trigger_bp_main_hand'),
(15036,'classic_spell_proc_trigger_bp_main_hand');

-- Melee hits done (swing 0x4, ability 0x10), damage, hit phase. Windfury Totem: "Each main hand hit has a 20% chance";
-- Flametongue Totem: every main hand hit (the 1.60 SpellAuraOptions proc chance does not decode).
DELETE FROM `spell_proc` WHERE `SpellId` IN (8515,10609,10612,8230,8250,10521,15036);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(8515,0,0,0,0,0,0,0x14,0,0x1,0x2,0,0,0,0,20,0,0),
(10609,0,0,0,0,0,0,0x14,0,0x1,0x2,0,0,0,0,20,0,0),
(10612,0,0,0,0,0,0,0x14,0,0x1,0x2,0,0,0,0,20,0,0),
(8230,0,0,0,0,0,0,0x14,0,0x1,0x2,0,0,0,0,100,0,0),
(8250,0,0,0,0,0,0,0x14,0,0x1,0x2,0,0,0,0,100,0,0),
(10521,0,0,0,0,0,0,0x14,0,0x1,0x2,0,0,0,0,100,0,0),
(15036,0,0,0,0,0,0,0x14,0,0x1,0x2,0,0,0,0,100,0,0);

-- The totems cast their passive (Totem::InitSummon casts spell index 0). Totem creatures from the 1.60 summon spells:
-- Windfury Totem 8512 / 10613 / 10614 -> 6112 / 7483 / 7484, Flametongue Totem 8227 / 8249 / 10526 / 16387 -> 5950 / 6012 / 7423 / 10557.
DELETE FROM `creature_template_spell` WHERE `CreatureID` IN (6112,7483,7484,5950,6012,7423,10557) AND `Index`=0;
INSERT INTO `creature_template_spell` (`CreatureID`,`Index`,`Spell`,`VerifiedBuild`) VALUES
(6112,0,8515,0),(7483,0,10609,0),(7484,0,10612,0),
(5950,0,8230,0),(6012,0,8250,0),(7423,0,10521,0),(10557,0,15036,0);

-- Overpower 7384 / 7887 / 11584 / 11585: "Only useable after the target dodges" (reactive combo point, Unit::ProcSkillsAndReactives)
DELETE FROM `spell_script_names` WHERE `ScriptName`='classic_spell_warr_overpower';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(7384,'classic_spell_warr_overpower'),
(7887,'classic_spell_warr_overpower'),
(11584,'classic_spell_warr_overpower'),
(11585,'classic_spell_warr_overpower');
