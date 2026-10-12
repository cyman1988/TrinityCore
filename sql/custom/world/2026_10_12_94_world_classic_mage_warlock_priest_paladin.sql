-- Classic 1.60: mage, warlock, priest and paladin spells whose client data is a dummy: script bindings and proc data
-- (src/server/scripts/Custom/Classic/world/classic_spells_mage_warlock_priest_paladin.cpp). Safe to run again.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_mage_cold_snap', 'classic_spell_mage_master_of_elements',
'classic_spell_mage_combustion', 'classic_spell_mage_combustion_crit', 'classic_spell_warl_nightfall', 'classic_spell_warl_pyroclasm',
'classic_spell_warl_pyroclasm_rain_of_fire', 'classic_spell_pri_holy_nova', 'classic_spell_pri_power_word_shield',
'classic_spell_pri_touch_of_weakness', 'classic_spell_pri_shadowguard', 'classic_spell_pal_judgement_of_light_wisdom',
'classic_spell_pal_judgement_of_command', 'classic_spell_pal_judgement_of_command_damage', 'classic_spell_pal_illumination');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Mage
(12472, 'classic_spell_mage_cold_snap'),
(29074, 'classic_spell_mage_master_of_elements'),
(11129, 'classic_spell_mage_combustion'),
(28682, 'classic_spell_mage_combustion_crit'),
-- Warlock
(18094, 'classic_spell_warl_nightfall'),
(18073, 'classic_spell_warl_pyroclasm'),
(5740, 'classic_spell_warl_pyroclasm_rain_of_fire'),
(6219, 'classic_spell_warl_pyroclasm_rain_of_fire'),
(11677, 'classic_spell_warl_pyroclasm_rain_of_fire'),
(11678, 'classic_spell_warl_pyroclasm_rain_of_fire'),
-- Priest
(15237, 'classic_spell_pri_holy_nova'),
(15430, 'classic_spell_pri_holy_nova'),
(15431, 'classic_spell_pri_holy_nova'),
(27799, 'classic_spell_pri_holy_nova'),
(27800, 'classic_spell_pri_holy_nova'),
(27801, 'classic_spell_pri_holy_nova'),
(17, 'classic_spell_pri_power_word_shield'),
(592, 'classic_spell_pri_power_word_shield'),
(600, 'classic_spell_pri_power_word_shield'),
(3747, 'classic_spell_pri_power_word_shield'),
(6065, 'classic_spell_pri_power_word_shield'),
(6066, 'classic_spell_pri_power_word_shield'),
(10898, 'classic_spell_pri_power_word_shield'),
(10899, 'classic_spell_pri_power_word_shield'),
(10900, 'classic_spell_pri_power_word_shield'),
(10901, 'classic_spell_pri_power_word_shield'),
(2652, 'classic_spell_pri_touch_of_weakness'),
(19261, 'classic_spell_pri_touch_of_weakness'),
(19262, 'classic_spell_pri_touch_of_weakness'),
(19264, 'classic_spell_pri_touch_of_weakness'),
(19265, 'classic_spell_pri_touch_of_weakness'),
(19266, 'classic_spell_pri_touch_of_weakness'),
(18137, 'classic_spell_pri_shadowguard'),
(19308, 'classic_spell_pri_shadowguard'),
(19309, 'classic_spell_pri_shadowguard'),
(19310, 'classic_spell_pri_shadowguard'),
(19311, 'classic_spell_pri_shadowguard'),
(19312, 'classic_spell_pri_shadowguard'),
-- Paladin
(20185, 'classic_spell_pal_judgement_of_light_wisdom'),
(20344, 'classic_spell_pal_judgement_of_light_wisdom'),
(20345, 'classic_spell_pal_judgement_of_light_wisdom'),
(20346, 'classic_spell_pal_judgement_of_light_wisdom'),
(20186, 'classic_spell_pal_judgement_of_light_wisdom'),
(20354, 'classic_spell_pal_judgement_of_light_wisdom'),
(20355, 'classic_spell_pal_judgement_of_light_wisdom'),
(20425, 'classic_spell_pal_judgement_of_command'),
(20961, 'classic_spell_pal_judgement_of_command'),
(20962, 'classic_spell_pal_judgement_of_command'),
(20967, 'classic_spell_pal_judgement_of_command'),
(20968, 'classic_spell_pal_judgement_of_command'),
(20467, 'classic_spell_pal_judgement_of_command_damage'),
(20963, 'classic_spell_pal_judgement_of_command_damage'),
(20964, 'classic_spell_pal_judgement_of_command_damage'),
(20965, 'classic_spell_pal_judgement_of_command_damage'),
(20966, 'classic_spell_pal_judgement_of_command_damage'),
(20210, 'classic_spell_pal_illumination');

-- Spirit of Redemption 20711 is a dummy aura in 1.60: the retail script hooks an absorb effect it does not have (handled by the
-- classic_unit_spirit_of_redemption UnitScript instead)
DELETE FROM `spell_script_names` WHERE `spell_id` = 20711 AND `ScriptName` = 'spell_pri_spirit_of_redemption';

-- Proc data. Chance 0 keeps the client chance, ProcFlags 0 the client flags, Charges 0 the client charges.
-- 29074 Master of Elements: Fire and Frost harmful spells, critical hits (the script reads the refund percentage)
-- 11129 Combustion: Fire harmful spell hits, 3 charges (only critical hits use one, see the script)
-- 18094 Nightfall: periodic damage ticks (the script checks the spell and rolls the talent amount)
-- 18073 Pyroclasm: Fire harmful spells, triggered ones too (Hellfire); effect 1 (the "Dummy Trigger" 18350) does not proc
-- Touch of Weakness: one charge ("the next melee attack"); the client data has none
-- 20210 Illumination: critical heals, triggered ones too (Holy Shock, Light's Vigil); effects 1-3 do not proc
-- Seal of Command: 7 procs per minute (vanilla); effect 2 (the judgement dummy) does not proc
DELETE FROM `spell_proc` WHERE `SpellId` IN (29074, 11129, 18094, 18073, 2652, 19261, 19262, 19264, 19265, 19266, 20210, 20375, 20915, 20918, 20919, 20920);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(29074, 20, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0),
(11129, 4, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0x3, 0, 0, 0, 100, 0, 3),
(18094, 0, 0, 0, 0, 0, 0, 0x40000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(18073, 4, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 2, 0x2, 0, 100, 0, 0),
(2652, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1),
(19261, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1),
(19262, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1),
(19264, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1),
(19265, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1),
(19266, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1),
(20210, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0x2, 2, 0xE, 0, 100, 0, 0),
(20375, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4, 7, 0, 0, 0),
(20915, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4, 7, 0, 0, 0),
(20918, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4, 7, 0, 0, 0),
(20919, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4, 7, 0, 0, 0),
(20920, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4, 7, 0, 0, 0);
