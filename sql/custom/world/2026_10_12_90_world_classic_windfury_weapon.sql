-- WoW Classic 1.60: Windfury Weapon. Enchants 283/284/525/1669/7569 equip 439431 on the imbued weapon; 20% per melee hit to cast
-- the rank's proc spell (2 extra attacks + attack power). 1.5 s cooldown from SpellAuraOptions keeps extra attacks from re-proccing.
DELETE FROM `spell_script_names` WHERE `spell_id` = 439431 AND `ScriptName` = 'classic_spell_sha_windfury_weapon';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(439431, 'classic_spell_sha_windfury_weapon');

DELETE FROM `spell_proc` WHERE `SpellId` = 439431;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(439431,0,0,0,0,0,0,0x14,0,1,2,0,0,0,0,20,1500,0);
