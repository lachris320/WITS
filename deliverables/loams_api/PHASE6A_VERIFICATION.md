# Phase 6a — manual server verification

Run against a deployed instance. Do NOT paste a real admin key into this file or any commit.

`BASE` = the deployed API root; `KEY` = a valid admin key (supplied by the operator at run
time via the shell env, never committed).

For each of the six reads, a request WITHOUT a key must return HTTP 401 + {"status":"error"},
and WITH a valid key must return 200 + data.

- [ ] search_students (JSON):        curl -sS -o /dev/null -w '%{http_code}\n' -X POST $BASE/search_students.php -H 'Content-Type: application/json' -d '{"search":""}'                 → 401
      curl ... -d '{"search":"","admin_key":"'"$KEY"'"}'                                                                                                                                → 200
- [ ] get_visitors (JSON):           -X POST -H 'Content-Type: application/json' -d '{}'  → 401 ;  -d '{"admin_key":"'"$KEY"'"}' → 200
- [ ] get_report_data (JSON):        -X POST -H 'Content-Type: application/json' -d '{}'  → 401 ;  with admin_key → 200
- [ ] get_report_time_data (JSON):   -X POST -H 'Content-Type: application/json' -d '{}'  → 401 ;  with admin_key → 200
- [ ] get_library_visits (form):     -X POST -d 'range=today'                             → 401 ;  -X POST "$BASE/get_library_visits.php?range=week" -d 'admin_key='"$KEY" → 200
- [ ] dashboard_summary (form):      -X POST                                              → 401 ;  -d 'admin_key='"$KEY" → 200

Regression (payload-aware helper's $_POST branch):
- [ ] A guarded WRITE still succeeds with admin_key in $_POST (e.g. delete_students with a throwaway id) → 200/expected error, NOT 401.

Kiosk/public must stay open (no key):
- [ ] student_login / guest_login / rfid_login / get_departments / get_branding → work with no key.
