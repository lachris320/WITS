-- turnstile_events
-- Correlation ledger between the turnstile controller (authoritative attendance
-- writer in turnstile.php) and the PowerShell bridge that injects the card into
-- the deployed legacy WITS.exe so it displays the student.
--
-- Lifecycle of a row:
--   created_at   set by turnstile.php on a successful ENTRY (same txn as attendance)
--   injected_at  set by the bridge's atomic POST claim, BEFORE it SendInputs the card;
--                released back to NULL only if the bridge aborts before SendInput
--   consumed_at  set by rfid_login.php when it recognises the injected scan and
--                SKIPS the duplicate attendance write
--
-- "injected"/"consumed" are deliberately not called "displayed": we cannot prove
-- WITS.exe rendered anything without modifying it. consumed_at only proves the
-- credential travelled back through the app's RFID API path.

CREATE TABLE IF NOT EXISTS turnstile_events (
    id                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    card              VARCHAR(64)      NOT NULL,
    controller_serial VARCHAR(32)      NULL,
    controller_index  VARCHAR(32)      NULL,
    reader            TINYINT UNSIGNED NOT NULL DEFAULT 0,

    created_at        DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    injected_at       DATETIME NULL,
    consumed_at       DATETIME NULL,

    PRIMARY KEY (id),
    -- Bridge poll: oldest pending (uninjected) recent event.
    INDEX idx_turnstile_pending (injected_at, created_at),
    -- rfid_login.php suppression lookup by card.
    INDEX idx_turnstile_consume (card, consumed_at, injected_at),
    -- turnstile.php retransmit idempotency: (Serial, Index) primary path and the
    -- (card, reader) fallback used when the firmware omits Serial/Index.
    INDEX idx_turnstile_retransmit (controller_serial, controller_index, created_at),
    INDEX idx_turnstile_retransmit_fb (card, reader, created_at)

    -- Retransmit de-duplication.
    -- PREFERRED once packet capture confirms the controller's Index is unique over
    -- the retention window: enable this UNIQUE KEY and let turnstile.php rely on the
    -- INSERT duplicate-key error (airtight, no SELECT-then-INSERT race). It is left
    -- OFF by default because a NON-unique Index would make it silently REJECT
    -- legitimate distinct entries — under-counting attendance, which our priority
    -- ordering (attendance correctness first) forbids. Until then turnstile.php does
    -- a SELECT ... FOR UPDATE idempotency check inside the transaction.
    --
    -- , UNIQUE KEY uq_turnstile_controller_event (controller_serial, controller_index)
);
