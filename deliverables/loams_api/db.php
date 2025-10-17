<?php
error_reporting(E_ALL);
ini_set('display_errors',1);

// db.php
$host = "localhost";
$user = "root";       // change if you set a MySQL user
$pass = "";           // change if you set a MySQL password
$db   = "wits_app";    // your database name

$conn = new mysqli($host, $user, $pass, $db);

if ($conn->connect_error) {
    die(json_encode(["status" => "error", "message" => "Database connection failed."]));
}
?>
