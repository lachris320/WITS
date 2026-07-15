<?php
/**
 * Date Window Helper
 * Shared date-range calculations for admin reporting endpoints
 * (dashboard_summary.php, get_library_visits.php) so "this week" and
 * "today" cannot silently drift between them.
 */

/**
 * Current Mon-Sun calendar week as a half-open [start, end) datetime range
 * (spec Section 5). date('N') = 1(Mon)..7(Sun); server-local time is
 * authoritative.
 *
 * @return array{0:string,1:string} [weekStart, weekEnd] as 'Y-m-d H:i:s'
 */
function week_window() {
    $dow = (int)date('N');
    $monday = new DateTime('today');
    $monday->modify('-' . ($dow - 1) . ' days');
    $start = $monday->format('Y-m-d 00:00:00');
    $end   = (clone $monday)->modify('+7 days')->format('Y-m-d 00:00:00');
    return [$start, $end];
}
?>
