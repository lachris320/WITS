s# WITS Quick Deployment Reference
**For fast deployment - refer to DEPLOYMENT_GUIDE.md for full details**

---

## 🚀 Quick Deploy - Local Network (30 Minutes)

### Server Setup (10 mins)

```batch
1. Install XAMPP
   → Download from apachefriends.org
   → Install to C:\xampp
   → Start Apache + MySQL

2. Create Database
   → Open http://localhost/phpmyadmin
   → Create database: wits_app
   → Import SQL schema (see DEPLOYMENT_GUIDE.md)

3. Deploy PHP Files
   → Copy loams_api folder to C:\xampp\htdocs\
   → Edit config.php (database credentials)
   → Test: http://localhost/loams_api/get_courses.php
```

### Client Setup (20 mins)

```batch
1. Build Release
   → Qt Creator: Switch to Release mode
   → Build → Rebuild All
   → File location: build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\WITS.exe

2. Deploy Qt Dependencies
   → Create folder: C:\WITS_Deploy
   → Copy WITS.exe to folder
   → Run: windeployqt --release C:\WITS_Deploy\WITS.exe

3. Configure
   → Create config.ini with server IP
   → Test application
```

---

## 📋 Pre-Flight Checklist

**Before deploying to users:**
- [ ] Application tested and working on dev machine
- [ ] Database created with all tables
- [ ] Sample data imported successfully
- [ ] Server IP address is static or reserved
- [ ] Firewall configured (port 80, 3306)
- [ ] All critical bugs fixed ✅
- [ ] Config files prepared
- [ ] Installer tested on clean Windows machine

---

## 🔧 Essential Commands

### Build Release
```batch
cd "C:\Users\ADMIN\OneDrive - Digiwiz\Documents\WITS"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build\Desktop_Qt_6_9_1_MinGW_64_bit-Release --target all
```

### Deploy Qt DLLs
```batch
cd C:\Qt\6.9.1\mingw_64\bin
windeployqt.exe --release "C:\WITS_Deploy\WITS.exe"
```

### Create Database Backup
```batch
C:\xampp\mysql\bin\mysqldump -u root -p wits_app > backup.sql
```

### Restore Database
```batch
C:\xampp\mysql\bin\mysql -u root -p wits_app < backup.sql
```

---

## 🌐 Network Configuration

### Find Server IP
```batch
ipconfig
# Look for IPv4 Address: 192.168.x.x
```

### Test Server Access (from client)
```batch
# In browser:
http://192.168.1.100/loams_api/get_courses.php

# Should return JSON data
```

### Configure Client
Edit `config.ini`:
```ini
[Server]
BaseURL=http://192.168.1.100/loams_api
```

---

## ⚡ One-Line Deployment Script

Save as `deploy.bat`:

```batch
@echo off
echo === WITS Quick Deploy ===

REM Step 1: Build Release
echo Building Release...
cd "C:\Users\ADMIN\OneDrive - Digiwiz\Documents\WITS"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build\Desktop_Qt_6_9_1_MinGW_64_bit-Release --target clean
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build\Desktop_Qt_6_9_1_MinGW_64_bit-Release --target all

REM Step 2: Create Deploy Folder
echo Creating deployment folder...
if not exist "C:\WITS_Deploy" mkdir "C:\WITS_Deploy"
del /Q "C:\WITS_Deploy\*.*"

REM Step 3: Copy Executable
echo Copying executable...
copy "build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\WITS.exe" "C:\WITS_Deploy\"

REM Step 4: Deploy Qt Dependencies
echo Deploying Qt dependencies...
cd C:\Qt\6.9.1\mingw_64\bin
windeployqt.exe --release "C:\WITS_Deploy\WITS.exe"

REM Step 5: Create Config
echo Creating config file...
(
echo [Server]
echo BaseURL=http://localhost/loams_api
echo [RFID]
echo Enabled=false
) > "C:\WITS_Deploy\config.ini"

echo.
echo === Deployment Complete! ===
echo Location: C:\WITS_Deploy
echo.
pause
```

---

## 🐛 Common Issues - Quick Fix

| Issue | Quick Fix |
|-------|-----------|
| Can't connect to server | Check `ipconfig` on server, update config.ini |
| Missing DLL error | Re-run windeployqt |
| Database error | Check XAMPP MySQL is running |
| RFID not working | Enable in Admin settings, install drivers |
| Upload fails | Check uploads folder permissions |

---

## 📱 Distribution Options

### Option 1: Network Install
```
Share: \\SERVER\WITS_Deploy
Users: Run \\SERVER\WITS_Deploy\WITS.exe
```

### Option 2: USB Install
```
Copy C:\WITS_Deploy to USB
Give to users
Run from USB or copy to C:\
```

### Option 3: Create Installer
```
Use Inno Setup (see DEPLOYMENT_GUIDE.md)
Creates single .exe installer
```

---

## 🔒 Security Quick Wins

```sql
-- Secure MySQL
ALTER USER 'root'@'localhost' IDENTIFIED BY 'new_secure_password';

-- Create app user
CREATE USER 'wits_user'@'localhost' IDENTIFIED BY 'app_password';
GRANT ALL ON wits_app.* TO 'wits_user'@'localhost';
FLUSH PRIVILEGES;
```

Update config.php:
```php
define('DB_USER', 'wits_user');
define('DB_PASS', 'app_password');
```

---

## 📊 Testing Script

Save as `test_deployment.bat`:

```batch
@echo off
echo === Testing WITS Deployment ===

echo.
echo [1] Testing server connection...
curl http://localhost/loams_api/get_courses.php
if %ERRORLEVEL% NEQ 0 (
    echo FAIL: Server not responding
) else (
    echo PASS: Server responding
)

echo.
echo [2] Testing database connection...
C:\xampp\mysql\bin\mysql -u root -p -e "USE wits_app; SELECT COUNT(*) FROM students;"
if %ERRORLEVEL% NEQ 0 (
    echo FAIL: Database connection failed
) else (
    echo PASS: Database connected
)

echo.
echo [3] Testing application...
if exist "C:\WITS_Deploy\WITS.exe" (
    echo PASS: Application exists
) else (
    echo FAIL: Application not found
)

echo.
echo === Test Complete ===
pause
```

---

## 🎯 Deployment Targets

### Single Computer (Development/Testing)
- XAMPP on same machine as app
- Use `localhost` in config
- Deploy time: ~15 minutes

### Small Office (2-10 users)
- XAMPP on one computer (server)
- Clients use server IP
- Deploy time: ~1 hour

### School Network (10-100 users)
- Dedicated server machine
- Static IP or DNS name
- Create installer for clients
- Deploy time: ~4 hours

### Multiple Locations
- Cloud hosting (AWS/Azure)
- VPN for security
- Centralized database
- Deploy time: ~1 day

---

## 📞 Quick Support

**App won't start:**
```
Check: Visual C++ Redistributable installed
Check: All DLLs in app folder
Check: platforms\qwindows.dll exists
```

**Can't connect:**
```
Test: ping server-ip
Test: http://server-ip in browser
Check: Firewall allows port 80
```

**Upload fails:**
```
Check: uploads folder exists
Check: folder permissions (write access)
Check: PHP max upload size in php.ini
```

---

## 🎓 Training Users (5 Minutes)

**Admin:**
1. Click "Admin Login" → Enter password
2. Use Database page to upload Excel with student data
3. Use Reporting page to generate reports
4. Use Settings to configure school info

**Students:**
1. Enter School ID
2. Click Login (or scan RFID card)
3. Photo appears when logged in

**Guests:**
1. Click "Guest Login"
2. Fill in name, company, purpose
3. Submit

---

## 📦 What's Included in Deployment

```
WITS_Deploy/
├── WITS.exe                    # Main application
├── config.ini                  # Configuration file
├── Qt6*.dll                    # Qt libraries (~20 files)
├── platforms/
│   └── qwindows.dll           # Windows platform plugin
├── styles/
│   └── qwindowsvistastyle.dll # Windows theme
├── imageformats/
│   └── *.dll                  # Image format plugins
└── translations/
    └── *.qm                   # Qt translations

Total Size: ~150-200 MB
```

---

## ✅ Final Checklist Before Go-Live

- [ ] Application tested on target Windows version
- [ ] Database backup created
- [ ] Server has static IP or DHCP reservation
- [ ] Config files customized (not using defaults)
- [ ] Sample data imported for testing
- [ ] Users trained on basic operations
- [ ] Admin password changed from default
- [ ] Support plan in place
- [ ] Rollback plan prepared

---

**Quick Deploy Guide Version:** 1.0
**For Full Details:** See DEPLOYMENT_GUIDE.md
**Last Updated:** 2025-10-18
