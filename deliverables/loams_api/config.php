<?php
/**
 * LOAMS Configuration File
 * Central configuration for database and environment settings
 */

// Database Configuration
define('DB_HOST', 'localhost');
define('DB_USER', 'root');
define('DB_PASS', '');
define('DB_NAME', 'wits_app');

// Environment Setting
// Set to 'development' for local testing, 'production' for live server
define('ENV', 'development');

// CORS Configuration
// Add your frontend domain(s) here. Use '*' only for development/testing
// For production, replace with your actual domain: 'https://yourdomain.com'
define('CORS_ALLOWED_ORIGIN', '*');

// File Upload Configuration
define('MAX_FILE_SIZE', 5 * 1024 * 1024); // 5MB
// Use const instead of define for array constants (PHP 5.6+)
const ALLOWED_IMAGE_EXTENSIONS = ['jpg', 'jpeg', 'png', 'gif'];
define('UPLOAD_DIR', 'loams_api.uploads/');

// Error Handling based on Environment
if (ENV === 'production') {
    // Production: Hide errors from users, log to file
    error_reporting(E_ALL);
    ini_set('display_errors', 0);
    ini_set('log_errors', 1);
    ini_set('error_log', __DIR__ . '/logs/php_errors.log');
} else {
    // Development: Show all errors
    error_reporting(E_ALL);
    ini_set('display_errors', 1);
}

// Create logs directory if it doesn't exist
if (!is_dir(__DIR__ . '/logs')) {
    @mkdir(__DIR__ . '/logs', 0755, true);
}
?>
