-- Classic 1.60: warrior, rogue and hunter spells whose client spell works through a dummy effect or dummy aura
-- (scripts in src/server/scripts/Custom/Classic/world/classic_spells_warrior_rogue_hunter.cpp). Safe to run again.

-- Retail TrinityCore scripts bound to the same spell ids by the base database (retail effect layouts, Validate fails or wrong behaviour)
DELETE FROM `spell_script_names` WHERE (`spell_id`, `ScriptName`) IN (
(23881,'spell_warr_bloodthirst'),(23881,'spell_warr_bloodthirst_enrage'),(23881,'spell_warr_cold_steel_hot_blood_bloodthirst'),
(23881,'spell_warr_deft_experience'),(23881,'spell_warr_vicious_contempt'),
(5308,'spell_warr_execute'),
(12292,'spell_warr_sweeping_strikes'),
(1943,'spell_rog_rupture'),
(2098,'spell_rog_eviscerate'),
(13877,'spell_rog_blade_flurry'));

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
'classic_spell_warr_execute','classic_spell_warr_bloodthirst','classic_spell_warr_deep_wounds','classic_spell_warr_deep_wound_bleed',
'classic_spell_warr_flurry','classic_spell_warr_sweeping_strikes','classic_spell_warr_retaliation','classic_spell_warr_last_stand',
'classic_spell_warr_berserker_rage','classic_spell_rog_eviscerate','classic_spell_rog_rupture','classic_spell_rog_blade_flurry',
'classic_spell_rog_relentless_strikes','classic_spell_rog_preparation','classic_spell_rog_remorseless_attacks','classic_spell_hun_mend_pet');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
-- warrior
(5308,'classic_spell_warr_execute'),
(20658,'classic_spell_warr_execute'),
(20660,'classic_spell_warr_execute'),
(20661,'classic_spell_warr_execute'),
(20662,'classic_spell_warr_execute'),
(23881,'classic_spell_warr_bloodthirst'),
(23892,'classic_spell_warr_bloodthirst'),
(23893,'classic_spell_warr_bloodthirst'),
(23894,'classic_spell_warr_bloodthirst'),
(12834,'classic_spell_warr_deep_wounds'),
(412609,'classic_spell_warr_deep_wound_bleed'),
(12319,'classic_spell_warr_flurry'),
(12292,'classic_spell_warr_sweeping_strikes'),
(20230,'classic_spell_warr_retaliation'),
(12975,'classic_spell_warr_last_stand'),
(18499,'classic_spell_warr_berserker_rage'),
-- rogue
(2098,'classic_spell_rog_eviscerate'),
(6760,'classic_spell_rog_eviscerate'),
(6761,'classic_spell_rog_eviscerate'),
(6762,'classic_spell_rog_eviscerate'),
(8623,'classic_spell_rog_eviscerate'),
(8624,'classic_spell_rog_eviscerate'),
(11299,'classic_spell_rog_eviscerate'),
(11300,'classic_spell_rog_eviscerate'),
(1943,'classic_spell_rog_rupture'),
(8639,'classic_spell_rog_rupture'),
(8640,'classic_spell_rog_rupture'),
(11273,'classic_spell_rog_rupture'),
(11274,'classic_spell_rog_rupture'),
(11275,'classic_spell_rog_rupture'),
(13877,'classic_spell_rog_blade_flurry'),
(14179,'classic_spell_rog_relentless_strikes'),
(14185,'classic_spell_rog_preparation'),
(14144,'classic_spell_rog_remorseless_attacks'),
-- hunter: Mend Pet ranks (Improved Mend Pet cleanse)
(136,'classic_spell_hun_mend_pet'),
(3111,'classic_spell_hun_mend_pet'),
(3661,'classic_spell_hun_mend_pet'),
(3662,'classic_spell_hun_mend_pet'),
(13542,'classic_spell_hun_mend_pet'),
(13543,'classic_spell_hun_mend_pet'),
(13544,'classic_spell_hun_mend_pet');

-- 12834 Deep Wounds, 12319 Flurry: melee critical hits (swings and abilities)
-- 12292 Sweeping Strikes, 13877 Blade Flurry: melee damage (swings and abilities); the script needs a second target, so charges are only
--   used when one is hit (Sweeping Strikes keeps its 5 client charges)
-- 20230 Retaliation: melee hits taken (swings and abilities), 30 client charges
-- 14179 Relentless Strikes: rogue abilities on cast; the script checks the combo points used and rolls the chance per point
-- 14144 Remorseless Attacks: kills that give experience or honor
DELETE FROM `spell_proc` WHERE `SpellId` IN (12834,12319,12292,13877,20230,14179,14144);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(12834,0,0,0,0,0,0,0x14,0,1,0x2,0x2,0,0,0,100,0,0),
(12319,0,0,0,0,0,0,0x14,0,1,0x2,0x2,0,0,0,100,0,0),
(12292,0,0,0,0,0,0,0x14,0,1,0x2,0,0,0,0,100,0,0),
(13877,0,0,0,0,0,0,0x14,0,1,0x2,0,0,0,0,100,0,0),
(20230,0,0,0,0,0,0,0x28,0,0,0x2,0,0,0,0,100,0,0),
(14179,0,8,0,0,0,0,0x15410,0,7,0x1,0,0,0,0,100,0,0),
(14144,0,0,0,0,0,0,0x2,0,0,0,0,0x1,0,0,100,0,0);

-- 412613 Deep Wound tick damage: a bleed, ignores armor (SPELL_ATTR0_CU_IGNORE_ARMOR)
DELETE FROM `spell_custom_attr` WHERE `entry` = 412613;
INSERT INTO `spell_custom_attr` (`entry`,`attributes`) VALUES
(412613,0x00008000);
