-- WoW Classic 1.60: druid and shaman class mechanics (classic_spells_druid_shaman.cpp).
-- Druid: Ferocious Bite (energy and attack power), Rip (attack power), Frenzied Regeneration, Swiftmend, Leader of the Pack and
-- Heart of the Wild in feral forms, Predatory Strikes refresh, Enrage armor reduction.
-- Shaman: Flametongue Weapon (1.60 imbue aura 436519), Lightning Shield damage.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_dru_ferocious_bite', 'classic_spell_dru_rip', 'classic_spell_dru_frenzied_regeneration', 'classic_spell_dru_swiftmend', 'classic_spell_dru_feral_form_talents', 'classic_spell_dru_predatory_strikes', 'classic_spell_dru_enrage', 'classic_spell_sha_flametongue_weapon', 'classic_spell_sha_lightning_shield');
-- retail scripts on the same ids (fail Validate or hook retail effects)
DELETE FROM `spell_script_names` WHERE (`spell_id`, `ScriptName`) IN (
(22568, 'spell_dru_ferocious_bite'),
(1079, 'spell_dru_rip'),
(22842, 'spell_dru_guardian_of_elune_healing'));
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(22568, 'classic_spell_dru_ferocious_bite'),
(22827, 'classic_spell_dru_ferocious_bite'),
(22828, 'classic_spell_dru_ferocious_bite'),
(22829, 'classic_spell_dru_ferocious_bite'),
(31018, 'classic_spell_dru_ferocious_bite'),
(1079, 'classic_spell_dru_rip'),
(9492, 'classic_spell_dru_rip'),
(9493, 'classic_spell_dru_rip'),
(9752, 'classic_spell_dru_rip'),
(9894, 'classic_spell_dru_rip'),
(9896, 'classic_spell_dru_rip'),
(22842, 'classic_spell_dru_frenzied_regeneration'),
(18562, 'classic_spell_dru_swiftmend'),
(768, 'classic_spell_dru_feral_form_talents'),
(5487, 'classic_spell_dru_feral_form_talents'),
(9634, 'classic_spell_dru_feral_form_talents'),
(16972, 'classic_spell_dru_predatory_strikes'),
(5229, 'classic_spell_dru_enrage'),
(436519, 'classic_spell_sha_flametongue_weapon'),
(324, 'classic_spell_sha_lightning_shield'),
(325, 'classic_spell_sha_lightning_shield'),
(905, 'classic_spell_sha_lightning_shield'),
(945, 'classic_spell_sha_lightning_shield'),
(8134, 'classic_spell_sha_lightning_shield'),
(10431, 'classic_spell_sha_lightning_shield'),
(10432, 'classic_spell_sha_lightning_shield');

-- Flametongue Weapon: every melee hit with the imbued weapon (the script checks the weapon).
-- Lightning Shield: the client data (melee, ranged and spell hits taken; 3 charges; 3.5 sec between procs), written out because the
-- client ProcChance does not decode reliably.
DELETE FROM `spell_proc` WHERE `SpellId` IN (436519, 324, 325, 905, 945, 8134, 10431, 10432);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(436519,0,0,0,0,0,0,0x14,0,1,2,0,0,0,0,100,0,0),
(324,0,0,0,0,0,0,0x222A8,0,1,2,0,0,0,0,100,3500,3),
(325,0,0,0,0,0,0,0x222A8,0,1,2,0,0,0,0,100,3500,3),
(905,0,0,0,0,0,0,0x222A8,0,1,2,0,0,0,0,100,3500,3),
(945,0,0,0,0,0,0,0x222A8,0,1,2,0,0,0,0,100,3500,3),
(8134,0,0,0,0,0,0,0x222A8,0,1,2,0,0,0,0,100,3500,3),
(10431,0,0,0,0,0,0,0x222A8,0,1,2,0,0,0,0,100,3500,3),
(10432,0,0,0,0,0,0,0x222A8,0,1,2,0,0,0,0,100,3500,3);
