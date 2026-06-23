<?php
/**
 * LOAMS API Router
 * Single entry point for all API requests
 */

// Set headers for JSON API
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, PUT, DELETE');
header('Access-Control-Allow-Headers: Content-Type');

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

// Get request method and path
$request_method = $_SERVER['REQUEST_METHOD'];
$request_uri = $_SERVER['REQUEST_URI'];

// Parse the endpoint (remove /api.php from path)
$path = str_replace('/api.php', '', parse_url($request_uri, PHP_URL_PATH));
$path = trim($path, '/');

// Log the request for debugging
error_log("API Request: $request_method $path");

// Route handler
try {
    switch ($path) {
        // ===== STUDENT ENDPOINTS =====
        case 'students/register':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'register_student.php';
            break;

        case 'students/check-duplicates':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'check_duplicates.php';
            break;

        // ===== DEPARTMENT ENDPOINTS =====
        case 'departments/list':
            if ($request_method !== 'GET') {
                throw new Exception('Method not allowed');
            }
            require_once 'get_departments.php';
            break;

        case 'departments/deactivate':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'deactivate_department.php';
            break;

        case 'departments/delete':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'delete_department.php';
            break;

        // ===== COURSE ENDPOINTS =====
        case 'courses/list':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'get_courses.php';
            break;

        // ===== VISIT ENDPOINTS =====
        case 'visits/reset':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'reset_visits.php';
            break;

        // ===== ADMIN ENDPOINTS =====
        case 'admin/update-key':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'update_admin_key.php';
            break;

        // ===== REPORT ENDPOINTS =====
        case 'reports/data':
            if ($request_method !== 'POST') {
                throw new Exception('Method not allowed');
            }
            require_once 'get_report_data.php';
            break;

        // ===== YEAR ENDPOINTS =====
        case 'years/list':
            if ($request_method !== 'GET') {
                throw new Exception('Method not allowed');
            }
            require_once 'get_years.php';
            break;

        // ===== DEFAULT - NOT FOUND =====
        default:
            http_response_code(404);
            echo json_encode([
                'status' => 'error',
                'message' => 'Endpoint not found',
                'requested_path' => $path
            ]);
            break;
    }
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode([
        'status' => 'error',
        'message' => $e->getMessage()
    ]);
    error_log("API Error: " . $e->getMessage());
}
?>