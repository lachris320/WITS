-- Branding engine persistence (Phase 1, PRD condition 5). One singleton row
-- (id = 1) holding the current branding state. New table only — no existing
-- table is altered.
CREATE TABLE IF NOT EXISTS branding_config (
  id           TINYINT UNSIGNED NOT NULL,
  theme_mode   VARCHAR(10)      NOT NULL DEFAULT 'auto',
  palette_json TEXT             NOT NULL,
  logo_hash    VARCHAR(64)      NOT NULL DEFAULT '',
  updated_at   TIMESTAMP        NOT NULL DEFAULT CURRENT_TIMESTAMP
                                ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
