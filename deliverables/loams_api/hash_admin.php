<?php
// Replace 'my-secret-key' with the admin key you want to hash
$adminKey = "admin123";

// Generate a secure hash
$hash = password_hash($adminKey, PASSWORD_DEFAULT);

// Print it out
echo "Your hashed admin key is: " . $hash;
