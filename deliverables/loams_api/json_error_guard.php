<?php
/**
 * Fail-closed JSON error guard for API endpoints. Functions only — no side
 * effects on include; an endpoint opts in by calling installJsonErrorGuard()
 * BEFORE `include "db.php"`.
 *
 * Why: db.php turns display_errors back ON and then does `new mysqli(...)`,
 * which on PHP 8.1+ throws mysqli_sql_exception when the DB is unreachable.
 * Uncaught, PHP prints "Fatal error: Uncaught mysqli_sql_exception ..." with the
 * DB name, user, file paths and stack trace straight into the JSON response.
 */

/**
 * Discard every pending output buffer, then send HTTP 500 and the generic
 * envelope {"status":"error","message":"Server error."}.
 */
function sendJsonServerError(): void
{
    while (ob_get_level() > 0) {
        ob_end_clean();
    }
    if (!headers_sent()) {
        http_response_code(500);
        header('Content-Type: application/json');
    }
    echo json_encode(["status" => "error", "message" => "Server error."]);
}

/**
 * Install the guard for the rest of the request:
 *  - display_errors off, log_errors on;
 *  - output is buffered, so anything printed before a failure (e.g. a warning
 *    shown while db.php has display_errors=1) can be thrown away;
 *  - warnings/notices are error_log()'d and never displayed, whatever
 *    display_errors is later set to (respects the @ operator);
 *  - an uncaught Throwable, or a fatal error caught at shutdown, is
 *    error_log()'d with $context and the client gets sendJsonServerError().
 */
function installJsonErrorGuard(string $context): void
{
    ini_set('display_errors', '0');
    ini_set('log_errors', '1');
    ob_start();

    set_error_handler(function (int $errno, string $errstr, string $errfile = '', int $errline = 0) use ($context): bool {
        if (!(error_reporting() & $errno)) {
            return false;   // silenced with @ (or filtered out): let PHP ignore it
        }
        error_log("$context: PHP error [$errno] $errstr in $errfile:$errline");
        return true;        // handled — never echoed into the response
    });

    set_exception_handler(function (Throwable $e) use ($context): void {
        error_log("$context: uncaught " . get_class($e) . ': ' . $e->getMessage()
            . ' in ' . $e->getFile() . ':' . $e->getLine());
        sendJsonServerError();
    });

    register_shutdown_function(function () use ($context): void {
        $err = error_get_last();
        $fatal = E_ERROR | E_PARSE | E_CORE_ERROR | E_COMPILE_ERROR | E_USER_ERROR;
        if ($err !== null && ($err['type'] & $fatal)) {
            error_log("$context: fatal [{$err['type']}] {$err['message']} in {$err['file']}:{$err['line']}");
            sendJsonServerError();
        }
    });
}
