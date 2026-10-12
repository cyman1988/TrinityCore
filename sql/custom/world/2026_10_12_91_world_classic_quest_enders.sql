-- WoW Classic 1.60: quests that could be accepted but never turned in (no quest ender)
-- 2986 Call of Water (Tauren shaman, from Telf Joolam 3066): ends at Islen Waterseer 5901 like 1528/1529/2985 (VMaNGOS)
DELETE FROM `creature_questender` WHERE `id` = 5901 AND `quest` = 2986;
INSERT INTO `creature_questender` (`id`, `quest`) VALUES (5901, 2986);

-- 92727 The Missing Scholar (Zephras Isle, from 253204): required itself as previous quest, and had no ender.
-- Completion text describes the Bloodstained Satchel 581822, which also starts the next step 92849.
UPDATE `quest_template_addon` SET `PrevQuestID` = 0 WHERE `ID` = 92727 AND `PrevQuestID` = 92727;
DELETE FROM `gameobject_questender` WHERE `id` = 581822 AND `quest` = 92727;
INSERT INTO `gameobject_questender` (`id`, `quest`) VALUES (581822, 92727);

-- 99048 A Missing Hand (Durotar, from Orgnil Soulscar 3142): the real ender, the body of Heglan Shadeeye 275657 north of
-- Tiragarde Keep, has no creature data yet. Until a sniff adds her, it can be turned in back at Orgnil Soulscar.
DELETE FROM `creature_questender` WHERE `id` = 3142 AND `quest` = 99048;
INSERT INTO `creature_questender` (`id`, `quest`) VALUES (3142, 99048);
