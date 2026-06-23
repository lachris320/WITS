# RFID Integration Setup Guide

## 📋 Overview
This guide will help you integrate RFID card scanning for automatic student login and attendance tracking.

## 🛠️ Hardware Requirements

### RFID Reader Options:
1. **USB RFID Reader** (Recommended for beginners)
   - Reads as keyboard input (HID mode)
   - Example: RC522, EM4100 USB reader
   - No special drivers needed
   - Price: $10-$30

2. **Serial RFID Reader** (What we implemented)
   - Connects via USB-to-Serial
   - Example: RDM6300, RC522 with serial adapter
   - Price: $5-$15

### RFID Cards/Tags:
- 125kHz cards (EM4100 format) - Most common
- 13.56MHz cards (MIFARE, NTAG) - More secure
- Make sure cards match your reader's frequency

## 🔧 Software Setup

### Step 1: Database Setup

1. Open phpMyAdmin or MySQL command line
2. Run the SQL script to add RFID column:

```bash
mysql -u root -p wits_db < add_rfid_column.sql
```

Or manually execute:
```sql
ALTER TABLE students
ADD COLUMN rfid_id VARCHAR(50) NULL DEFAULT NULL AFTER school_id,
ADD UNIQUE INDEX idx_rfid_id (rfid_id);
```

### Step 2: PHP Backend Setup

1. Copy `rfid_login.php` to your web server's PHP directory
   - XAMPP: `C:\xampp\htdocs\`
   - WAMP: `C:\wamp\www\`

2. Update database credentials in `rfid_login.php`:
```php
$servername = "localhost";
$username = "root";      // Your MySQL username
$password = "";          // Your MySQL password
$dbname = "wits_db";     // Your database name
```

3. Test the endpoint:
   - Visit: `http://localhost/loams_api/rfid_login.php`
   - You should see a JSON error (expected without POST data)

### Step 3: Qt Application Build

1. The RFID files are already added to CMakeLists.txt
2. Clean and rebuild the project in Qt Creator:
   - Build → Clean All
   - Build → Build All

### Step 4: Hardware Connection

#### For USB RFID Readers:
1. Plug the RFID reader into a USB port
2. Windows should auto-detect it as a COM port
3. Check Device Manager → Ports (COM & LPT)
4. Note the COM port number (e.g., COM3, COM5)

#### For Testing Without Hardware:
You can test by manually entering RFID IDs in the username field (if they exist in the database).

## 📊 Registering RFID Cards to Students

### Method 1: Direct Database Update
```sql
UPDATE students
SET rfid_id = '0123456789AB'
WHERE school_id = '2024001';
```

### Method 2: Add RFID field to Admin Panel
You can extend the admin panel to allow assigning RFID cards:
1. Add an RFID input field in the student edit dialog
2. Scan the card to get its ID
3. Save it to the database

### Method 3: Bulk Import via Excel
Add an `rfid_id` column to your student Excel import template.

## 🧪 Testing the RFID System

### Test Without Hardware:
1. Add a test RFID to database:
```sql
UPDATE students
SET rfid_id = 'TEST123'
WHERE school_id = '2024001';
```

2. Modify `rfidreader.cpp` temporarily for testing:
```cpp
// In RFIDReader constructor, add a test timer
QTimer *testTimer = new QTimer(this);
connect(testTimer, &QTimer::timeout, this, [this]() {
    emit rfidDetected("TEST123");
});
testTimer->start(5000); // Emit test RFID every 5 seconds
```

### Test With Hardware:
1. Run the application
2. Check Application Output in Qt Creator
3. You should see: `RFID Reader connected on port: COMX`
4. Scan a card
5. You should see: `RFID detected: [card ID]`
6. Student should auto-login if RFID is registered

## 🔍 Troubleshooting

### RFID Reader Not Detected
**Problem:** "No RFID reader detected" in logs

**Solutions:**
1. Check USB connection
2. Install CH340/CP2102 drivers if needed
3. Manually specify COM port in code:
```cpp
rfidReader->connectToReader("COM3"); // Replace COM3 with your port
```

### RFID Not Reading
**Problem:** Reader connected but no scans detected

**Solutions:**
1. Check baud rate in `rfidreader.cpp` (try 9600, 115200)
2. Verify card frequency matches reader
3. Test reader with manufacturer software

### Student Not Found
**Problem:** RFID scans but student doesn't login

**Solutions:**
1. Check if RFID ID exists in database:
```sql
SELECT * FROM students WHERE rfid_id = '[scanned_id]';
```
2. RFID IDs are case-sensitive
3. Check for extra spaces/characters

### Permission Denied Error
**Problem:** Can't open COM port

**Solutions:**
1. Close other programs using the port
2. Run application as Administrator
3. Check port permissions in Device Manager

## 📱 Common RFID Reader Types

### 1. Keyboard Emulation (HID)
- Easiest to use
- Works like keyboard input
- Modify code to read from `QLineEdit` input

### 2. Serial/UART (What we use)
- More flexible
- Background operation
- Current implementation

### 3. Wiegand Protocol
- Common in access control
- Requires GPIO or special adapter

## 🎯 Recommended Workflow

1. **Initial Setup:**
   - Buy USB RFID reader kit with cards
   - Set up database
   - Deploy PHP backend

2. **Student Registration:**
   - Issue RFID cards to students
   - Scan card to get ID
   - Add to student record in database

3. **Daily Use:**
   - Students tap card on reader
   - Auto-login and attendance recorded
   - No typing needed!

## 🔐 Security Considerations

- RFID IDs should be unique in database
- Consider encrypting RFID data
- Log all access attempts
- Implement rate limiting for failed scans

## 📞 Support

If you encounter issues:
1. Check Application Output logs
2. Test PHP endpoint directly
3. Verify database connection
4. Check RFID hardware with test software

---

**Hardware Recommendations:**
- **Budget:** HiLetgo RC522 + USB adapter (~$8)
- **Mid-range:** RDM6300 USB Reader (~$15)
- **Professional:** ACR122U NFC Reader (~$40)
