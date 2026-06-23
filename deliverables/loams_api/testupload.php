<?php
echo "<h2>PHP Upload Configuration</h2>";
echo "upload_max_filesize: " . ini_get('upload_max_filesize') . "<br>";
echo "post_max_size: " . ini_get('post_max_size') . "<br>";
echo "max_file_uploads: " . ini_get('max_file_uploads') . "<br>";
echo "memory_limit: " . ini_get('memory_limit') . "<br>";

$uploadDir = "uploads/";
if (!is_dir($uploadDir)) {
    if (mkdir($uploadDir, 0777, true)) {
        echo "<br>✅ 'uploads' folder created successfully!";
    } else {
        echo "<br>❌ Failed to create 'uploads' folder. Check permissions.";
    }
} else {
    echo "<br>✅ 'uploads' folder already exists.";
}

if (is_writable($uploadDir)) {
    echo "<br>✅ 'uploads' folder is writable.";
} else {
    echo "<br>❌ 'uploads' folder is NOT writable. Check permissions.";
}
?>