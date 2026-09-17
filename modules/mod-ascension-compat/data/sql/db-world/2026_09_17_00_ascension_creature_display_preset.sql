-- Storage for the creature display presets AscensionCreaturePresetMgr::LoadFromDB reads.
--
-- The preset support added with the mirror-image NPC feature shipped without this table, so
-- the module's unconditional SELECT hit error 1146 and MySQLConnection::_HandleMySQLErrno
-- aborted worldserver during startup on every realm whose world database predates it.
--
-- A preset describes the player-like appearance a creature entry is shown with: the display
-- id it applies to (0 = the entry's default), the character appearance fields the mirror-image
-- packet carries, and the item display slots in the order the manager stores them.

CREATE TABLE IF NOT EXISTS `creature_display_preset` (
  `entry`          INT UNSIGNED     NOT NULL,
  `display_id`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `race`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `gender`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `class`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `skin`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `face`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `hair`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `haircolor`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `facialhair`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `guild_id`       INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_head`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_shoulders` INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_body`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_chest`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_waist`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_legs`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_feet`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_wrists`    INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_hands`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_back`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `item_tabard`    INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`entry`, `display_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
