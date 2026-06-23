# WITS Application - Complete Deployment Guide
**Version:** 1.0
**Platform:** Windows 10/11
**Qt Version:** 6.9.1
**Database:** MySQL/MariaDB
**Web Server:** Apache (XAMPP)

---

## Table of Contents
1. [Pre-Deployment Checklist](#pre-deployment-checklist)
2. [Backend Deployment (PHP/MySQL)](#backend-deployment)
3. [Frontend Deployment (Qt Application)](#frontend-deployment)
4. [Database Setup](#database-setup)
5. [Configuration](#configuration)
6. [Creating Installer](#creating-installer)
7. [Distribution](#distribution)
8. [Troubleshooting](#troubleshooting)

---

## Pre-Deployment Checklist

### ✅ Before You Deploy

- [ ] All critical bugs fixed (completed ✅)
- [ ] Application tested on development machine
- [ ] Database schema finalized
- [ ] API endpoints tested and working
- [ ] Configuration files prepared
- [ ] Test data prepared (sample students, photos)
- [ ] Deployment machine meets minimum requirements

### Minimum Requirements

**Server/Hosting Machine:**
- Windows Server 2016+ or Windows 10/11
- 4GB RAM minimum (8GB recommended)
- 20GB free disk space
- MySQL 5.7+ or MariaDB 10.3+
- PHP 7.4 - 8.2
- Apache 2.4+

**Client Machines:**
- Windows 10/11 (64-bit)
- 2GB RAM minimum
- 500MB free disk space
- Network connectivity to server
- Screen resolution: 1024x768 minimum (1920x1080 recommended)

---

## Backend Deployment

### Step 1: Choose Deployment Method

#### **Option A: XAMPP (Recommended for Small/Medium Scale)**

**Advantages:**
- Easy to install and configure
- All-in-one package (Apache + MySQL + PHP)
- Good for local network deployments
- Free and open source

**Disadvantages:**
- Not optimized for production
- Less secure than dedicated setup
- Limited scalability

#### **Option B: Dedicated Server (Recommended for Production)**

**Advantages:**
- Better performance
- Enhanced security
- Professional hosting
- Scalable

**Disadvantages:**
- More complex setup
- Costs money
- Requires technical knowledge

---

### Step 2: Backend Setup (XAMPP Method)

#### 2.1 Install XAMPP on Server

1. **Download XAMPP**
   - Visit: https://www.apachefriends.org/download.html
   - Download version with PHP 8.0 or 8.1
   - File size: ~150MB

2. **Install XAMPP**
   ```
   - Run installer as Administrator
   - Install to: C:\xampp (default)
   - Select components: Apache, MySQL, PHP, phpMyAdmin
   - Complete installation
   ```

3. **Start Services**
   ```
   - Open XAMPP Control Panel as Administrator
   - Start Apache
   - Start MySQL
   - Check that both show green "Running" status
   ```

#### 2.2 Deploy PHP Backend

1. **Copy API Files**
   ```
   Source: C:\xampp\htdocs\loams_api\*
   Destination: C:\xampp\htdocs\loams_api\

   Files to copy:
   - config.php
   - db.php
   - student_login.php
   - guest_login.php
   - rfid_login.php
   - register_student.php
   - get_courses.php
   - get_students.php
   - update_student.php
   - delete_students.php
   - bulk_update.php
   - get_report_data.php
   - get_visitor_logs.php
   - (all other PHP files)
   ```

2. **Create Uploads Directory**
   ```batch
   mkdir C:\xampp\htdocs\loams_api\uploads
   mkdir C:\xampp\htdocs\loams_api\logs
   ```

3. **Set Directory Permissions**
   ```
   Right-click on folders → Properties → Security
   - loams_api\uploads: Full Control for Apache user
   - loams_api\logs: Full Control for Apache user
   ```

#### 2.3 Configure PHP Backend

1. **Edit config.php**
   ```php
   <?php
   // Database Configuration
   define('DB_HOST', 'localhost');
   define('DB_USER', 'root');
   define('DB_PASS', 'your_secure_password');  // CHANGE THIS!
   define('DB_NAME', 'wits_app');

   // Environment Setting
   define('ENV', 'production');  // CHANGE from 'development'

   // CORS Configuration
   define('CORS_ALLOWED_ORIGIN', 'http://your-server-ip');  // CHANGE from '*'

   // File Upload Configuration
   define('MAX_FILE_SIZE', 5 * 1024 * 1024); // 5MB
   const ALLOWED_IMAGE_EXTENSIONS = ['jpg', 'jpeg', 'png', 'gif'];
   define('UPLOAD_DIR', 'loams_api/uploads/');
   ?>
   ```

2. **Secure MySQL**
   ```
   - Open XAMPP Control Panel
   - Click "Shell" button
   - Run: mysql_secure_installation
   - Set root password
   - Remove anonymous users
   - Disallow root login remotely
   - Remove test database
   ```

3. **Configure Apache (Optional - for remote access)**

   Edit `C:\xampp\apache\conf\extra\httpd-xampp.conf`:
   ```apache
   # Allow remote access to server
   <Directory "C:/xampp/htdocs">
       Require all granted  # Allow from all IPs
   </Directory>
   ```

   **Security Note:** For production, restrict to specific IPs:
   ```apache
   Require ip 192.168.1.0/24  # Allow only local network
   ```

4. **Enable HTTPS (Recommended for Production)**

   In XAMPP Control Panel:
   ```
   - Click "Config" next to Apache
   - Select "httpd-ssl.conf"
   - Uncomment: Include conf/extra/httpd-ssl.conf
   - Restart Apache
   ```

---

## Database Setup

### Step 1: Create Database

1. **Open phpMyAdmin**
   - Navigate to: http://localhost/phpmyadmin
   - Login with root credentials

2. **Create Database**
   ```sql
   CREATE DATABASE wits_app CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
   ```

3. **Create Tables**

   ```sql
   USE wits_app;

   -- Students table
   CREATE TABLE students (
       id INT AUTO_INCREMENT PRIMARY KEY,
       school_id VARCHAR(50) UNIQUE NOT NULL,
       code VARCHAR(50) UNIQUE,  -- RFID code
       name VARCHAR(100) NOT NULL,
       course VARCHAR(100),
       department VARCHAR(100),
       year_level VARCHAR(20),
       gender ENUM('Male', 'Female', 'Other'),
       status ENUM('Active', 'Inactive') DEFAULT 'Active',
       photo VARCHAR(255),
       visits INT DEFAULT 0,
       created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
       updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
       INDEX idx_school_id (school_id),
       INDEX idx_code (code),
       INDEX idx_department (department),
       INDEX idx_name (name)
   ) ENGINE=InnoDB;

   -- Library visits table
   CREATE TABLE library_visits (
       id INT AUTO_INCREMENT PRIMARY KEY,
       student_id VARCHAR(50),
       course VARCHAR(100),
       login_time DATETIME NOT NULL,
       yearlog INT,
       year_level VARCHAR(20),
       department VARCHAR(100),
       name VARCHAR(100),
       gender VARCHAR(20),
       status VARCHAR(20),
       photo VARCHAR(255),
       INDEX idx_student_id (student_id),
       INDEX idx_login_time (login_time),
       INDEX idx_department (department),
       INDEX idx_yearlog (yearlog),
       FOREIGN KEY (student_id) REFERENCES students(school_id) ON DELETE SET NULL
   ) ENGINE=InnoDB;

   -- Guest visitors table
   CREATE TABLE guest_visitors (
       id INT AUTO_INCREMENT PRIMARY KEY,
       name VARCHAR(100) NOT NULL,
       contact VARCHAR(100),
       company VARCHAR(100),
       purpose TEXT,
       visit_time DATETIME DEFAULT CURRENT_TIMESTAMP,
       INDEX idx_visit_time (visit_time),
       INDEX idx_name (name)
   ) ENGINE=InnoDB;

   -- Courses table
   CREATE TABLE courses (
       id INT AUTO_INCREMENT PRIMARY KEY,
       department VARCHAR(100) NOT NULL,
       course_name VARCHAR(100) NOT NULL,
       UNIQUE KEY unique_dept_course (department, course_name)
   ) ENGINE=InnoDB;
   ```

4. **Insert Sample Data (Optional)**
   ```sql
   -- Sample departments and courses
   INSERT INTO courses (department, course_name) VALUES
   ('Computer Science', 'BS Computer Science'),
   ('Engineering', 'BS Civil Engineering'),
   ('Business', 'BS Business Administration'),
   ('Education', 'BS Elementary Education');

   -- Sample student
   INSERT INTO students (school_id, code, name, course, department, year_level, gender, status) VALUES
   ('2024001', 'TEST123', 'John Doe', 'BS Computer Science', 'Computer Science', '1st Year', 'Male', 'Active');
   ```

### Step 2: Create Database User (Security)

```sql
-- Create dedicated user for application (more secure than using root)
CREATE USER 'wits_user'@'localhost' IDENTIFIED BY 'secure_password_here';

-- Grant necessary permissions
GRANT SELECT, INSERT, UPDATE, DELETE ON wits_app.* TO 'wits_user'@'localhost';

-- Apply changes
FLUSH PRIVILEGES;
```

Update `config.php` with new credentials:
```php
define('DB_USER', 'wits_user');
define('DB_PASS', 'secure_password_here');
```

### Step 3: Backup Strategy

Create a backup script `C:\xampp\htdocs\loams_api\backup_db.bat`:
```batch
@echo off
set TIMESTAMP=%date:~-4,4%%date:~-7,2%%date:~-10,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%
C:\xampp\mysql\bin\mysqldump -u root -p wits_app > C:\xampp\backups\wits_app_%TIMESTAMP%.sql
echo Backup completed: wits_app_%TIMESTAMP%.sql
```

**Schedule daily backups using Windows Task Scheduler.**

---

## Frontend Deployment

### Step 1: Build Release Version

1. **Open Qt Creator**
   - Open your WITS project
   - Switch to "Release" build mode (bottom left)

2. **Clean and Rebuild**
   ```
   Build → Clean All
   Build → Rebuild All
   ```

3. **Locate Executable**
   ```
   Path: C:\Users\ADMIN\OneDrive - Digiwiz\Documents\WITS\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\
   File: WITS.exe
   ```

### Step 2: Deploy Qt Dependencies

Qt applications need DLLs to run on machines without Qt installed.

#### 2.1 Create Deployment Folder

```batch
mkdir C:\WITS_Deployment
mkdir C:\WITS_Deployment\WITS
```

#### 2.2 Copy Executable

```batch
copy "C:\Users\ADMIN\OneDrive - Digiwiz\Documents\WITS\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\WITS.exe" "C:\WITS_Deployment\WITS\"
```

#### 2.3 Run windeployqt

Open Command Prompt as Administrator:

```batch
cd C:\Qt\6.9.1\mingw_64\bin

windeployqt.exe --release --compiler-runtime "C:\WITS_Deployment\WITS\WITS.exe"
```

This automatically copies all required Qt DLLs, plugins, and translations.

#### 2.4 Verify Deployment

Your `C:\WITS_Deployment\WITS\` folder should now contain:
```
WITS.exe
Qt6Core.dll
Qt6Gui.dll
Qt6Widgets.dll
Qt6Network.dll
Qt6Charts.dll
Qt6OpenGL.dll
Qt6OpenGLWidgets.dll
Qt6SerialPort.dll
platforms\qwindows.dll
styles\qwindowsvistastyle.dll
imageformats\*.dll
(and other necessary files)
```

**Total size:** Approximately 150-200 MB

### Step 3: Create Configuration File

Create `C:\WITS_Deployment\WITS\config.ini`:

```ini
[Server]
BaseURL=http://192.168.1.100/loams_api
Timeout=5000

[Application]
SchoolName=Your School Name
WindowTitle=WITS - Library Management System

[RFID]
Enabled=false
AutoConnect=true
BaudRate=9600

[Features]
GuestLogin=true
```

### Step 4: Update Application to Use Config File

Add to mainwindow.cpp (at the top of constructor):

```cpp
// Load configuration
QSettings settings("config.ini", QSettings::IniFormat);
QString apiBase = settings.value("Server/BaseURL", "http://localhost/loams_api").toString();

// Use apiBase for all network requests instead of hardcoded localhost
```

---

## Creating Installer

### Method 1: Using Inno Setup (Recommended)

1. **Download Inno Setup**
   - Visit: https://jrsoftware.org/isdl.php
   - Download and install Inno Setup 6.x

2. **Create Installation Script**

Create `WITS_Installer.iss`:

```iss
[Setup]
AppName=WITS Library Management System
AppVersion=1.0
DefaultDirName={autopf}\WITS
DefaultGroupName=WITS
OutputDir=C:\WITS_Deployment
OutputBaseFilename=WITS_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "C:\WITS_Deployment\WITS\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\WITS"; Filename: "{app}\WITS.exe"
Name: "{autodesktop}\WITS"; Filename: "{app}\WITS.exe"

[Run]
Filename: "{app}\WITS.exe"; Description: "Launch WITS"; Flags: postinstall nowait skipifsilent
```

3. **Compile Installer**
   ```
   - Open Inno Setup
   - File → Open → Select WITS_Installer.iss
   - Build → Compile
   - Output: C:\WITS_Deployment\WITS_Setup.exe
   ```

**Installer size:** Approximately 160-220 MB

### Method 2: Using NSIS

1. **Download NSIS**
   - Visit: https://nsis.sourceforge.io/Download
   - Install NSIS 3.x

2. **Create Script** (similar to Inno Setup)

### Method 3: Simple ZIP Distribution

For quick deployment without installer:

```batch
cd C:\WITS_Deployment
"C:\Program Files\7-Zip\7z.exe" a -tzip WITS_Portable.zip WITS\*
```

Users extract and run WITS.exe directly.

---

## Configuration

### Server Configuration

#### 1. Find Server IP Address

On server machine:
```batch
ipconfig

# Look for IPv4 Address under your active network adapter
# Example: 192.168.1.100
```

#### 2. Test Server Access

From client machine:
```
Open browser
Navigate to: http://192.168.1.100/loams_api/config.php
Should see blank page (not error)
```

#### 3. Configure Firewall

**On Server:**
```
Control Panel → Windows Defender Firewall → Advanced Settings
→ Inbound Rules → New Rule
→ Port → TCP → Specific Ports: 80, 443, 3306
→ Allow the connection
→ All profiles
→ Name: WITS Server Access
```

**On Router (if accessing from different network):**
- Port forward 80 → Server IP
- Port forward 443 → Server IP (for HTTPS)

### Client Configuration

Edit `config.ini` on each client:

```ini
[Server]
BaseURL=http://192.168.1.100/loams_api  # Use actual server IP
```

---

## Distribution

### Option 1: Network Share

1. **Create Network Share on Server**
   ```
   - Right-click C:\WITS_Deployment
   - Properties → Sharing → Advanced Sharing
   - Share this folder
   - Set Permissions (Read for all users)
   - Note the network path: \\SERVER-NAME\WITS_Deployment
   ```

2. **Install on Clients**
   ```
   - Access: \\SERVER-NAME\WITS_Deployment
   - Run: WITS_Setup.exe
   - Follow installation wizard
   ```

### Option 2: USB Distribution

1. Copy to USB:
   ```
   Copy C:\WITS_Deployment\WITS_Setup.exe to USB drive
   ```

2. Install on each machine from USB

### Option 3: Cloud Distribution

1. Upload to Google Drive/Dropbox
2. Share download link
3. Users download and install

---

## Testing Deployment

### Server Testing

1. **Test PHP Backend**
   ```
   Navigate to: http://server-ip/loams_api/get_courses.php
   Expected: JSON response with departments/courses
   ```

2. **Test Database Connection**
   ```php
   Create test.php in C:\xampp\htdocs\:

   <?php
   $conn = new mysqli('localhost', 'wits_user', 'password', 'wits_app');
   if ($conn->connect_error) {
       die("Connection failed: " . $conn->connect_error);
   }
   echo "Database connected successfully!";
   ?>
   ```

3. **Test File Upload**
   ```
   Create test file in uploads directory
   Verify Apache can read/write
   ```

### Client Testing

1. **Install on Test Machine**
   - Fresh Windows 10/11 installation (virtual machine recommended)
   - Run installer
   - Launch application

2. **Test All Features**
   - [ ] Student login with school ID
   - [ ] Guest login
   - [ ] Database upload (Excel + photos)
   - [ ] Student search and edit
   - [ ] Report generation (PDF/Excel)
   - [ ] Visitor log export
   - [ ] Admin settings save/load

3. **Test Network Scenarios**
   - [ ] Server offline → Should show error gracefully
   - [ ] Slow network → Should timeout appropriately
   - [ ] Invalid credentials → Should show error

### Performance Testing

1. **Load Testing**
   ```
   - Import 1000+ student records
   - Generate large reports
   - Monitor response times
   ```

2. **Concurrent Users**
   ```
   - Multiple clients accessing simultaneously
   - Check for database locks
   - Monitor server resource usage
   ```

---

## Troubleshooting

### Common Issues

#### Issue 1: "Cannot connect to server"

**Symptoms:** Application shows network error

**Solutions:**
1. Check server IP in config.ini
2. Verify Apache is running (XAMPP Control Panel)
3. Check firewall settings
4. Test URL in browser: http://server-ip/loams_api/
5. Verify client has network access to server (ping server-ip)

#### Issue 2: "Invalid server response"

**Symptoms:** Application receives error instead of data

**Solutions:**
1. Check PHP error logs: `C:\xampp\apache\logs\error.log`
2. Verify config.php settings
3. Check database connection
4. Enable error display temporarily in config.php:
   ```php
   define('ENV', 'development');
   ```

#### Issue 3: "Could not load Qt platform plugin"

**Symptoms:** Application won't start, shows plugin error

**Solutions:**
1. Re-run windeployqt
2. Check platforms folder exists and contains qwindows.dll
3. Verify all Qt DLLs are present
4. Install Visual C++ Redistributable if needed

#### Issue 4: "Database connection failed"

**Symptoms:** Cannot access database

**Solutions:**
1. Check MySQL is running (XAMPP Control Panel)
2. Verify credentials in config.php
3. Test connection with phpMyAdmin
4. Check user permissions:
   ```sql
   SHOW GRANTS FOR 'wits_user'@'localhost';
   ```

#### Issue 5: RFID Reader Not Detected

**Symptoms:** RFID feature doesn't work

**Solutions:**
1. Enable RFID in admin settings
2. Install CH340/FT232 drivers if needed
3. Check COM port in Device Manager
4. Verify RFID checkbox in admin panel

### Debug Mode

Enable detailed logging:

1. Create `logs` directory in application folder
2. Add to code:
   ```cpp
   qSetMessagePattern("[%{time}] %{type}: %{message}");
   QLoggingCategory::setFilterRules("*.debug=true");
   ```

---

## Security Best Practices

### Server Security

1. **Change Default Passwords**
   ```
   - MySQL root password
   - phpMyAdmin password
   - Create dedicated database user
   ```

2. **Disable Directory Listing**
   Edit `httpd.conf`:
   ```apache
   Options -Indexes
   ```

3. **Enable HTTPS**
   ```
   - Obtain SSL certificate (Let's Encrypt)
   - Configure Apache SSL
   - Update config.ini to use HTTPS URLs
   ```

4. **Restrict Database Access**
   ```sql
   -- Allow only local connections
   CREATE USER 'wits_user'@'localhost' IDENTIFIED BY 'password';
   -- NOT: 'wits_user'@'%' (allows any host)
   ```

5. **Regular Updates**
   ```
   - Update XAMPP regularly
   - Update PHP
   - Update MySQL
   - Apply Windows security patches
   ```

### Application Security

1. **Input Validation**
   - Already implemented in PHP backend
   - Client-side validation as first defense

2. **Secure Configuration**
   ```ini
   [Server]
   BaseURL=https://your-domain.com/loams_api  # Use HTTPS
   ```

3. **File Upload Security**
   - Limit file sizes (5MB default)
   - Validate file types
   - Scan for malware (optional)

---

## Maintenance

### Daily Tasks
- [ ] Check server is running
- [ ] Monitor disk space
- [ ] Review error logs

### Weekly Tasks
- [ ] Backup database
- [ ] Review application logs
- [ ] Test critical functions

### Monthly Tasks
- [ ] Update software (if updates available)
- [ ] Archive old logs
- [ ] Review user feedback
- [ ] Performance optimization

### Backup Procedure

**Automated Backup:**
```batch
# Create scheduled task to run daily at 2 AM
schtasks /create /tn "WITS Database Backup" /tr "C:\xampp\htdocs\loams_api\backup_db.bat" /sc daily /st 02:00
```

**Manual Backup:**
```batch
# Database
C:\xampp\mysql\bin\mysqldump -u root -p wits_app > backup_%date%.sql

# Uploaded files
xcopy C:\xampp\htdocs\loams_api\uploads C:\Backups\uploads /E /I /Y
```

---

## Scaling for Larger Deployments

### For 100+ Users

1. **Upgrade to Dedicated Server**
   - Windows Server or Linux
   - Dedicated MySQL server
   - Load balancer for multiple app servers

2. **Database Optimization**
   ```sql
   -- Add more indexes
   -- Enable query caching
   -- Use InnoDB for all tables
   ```

3. **Cloud Hosting**
   - AWS, Azure, or DigitalOcean
   - RDS for database
   - S3 for file storage
   - CloudFront for CDN

### For Multiple Locations

1. **Centralized Database**
   - Single database accessible from all locations
   - VPN for secure access
   - Database replication for redundancy

2. **API Gateway**
   - Centralized API endpoint
   - Rate limiting
   - Authentication tokens

---

## Support and Updates

### Creating Update Package

1. **Build new version**
2. **Run windeployqt**
3. **Create installer with new version number**
4. **Test on clean machine**
5. **Distribute to users**

### Version Control

Tag releases:
```
v1.0.0 - Initial release
v1.1.0 - Added feature X
v1.1.1 - Bug fix Y
```

---

## Quick Start Checklist

### Server Setup (30 minutes)
- [ ] Install XAMPP
- [ ] Create database
- [ ] Import schema
- [ ] Copy PHP files
- [ ] Configure config.php
- [ ] Test endpoints

### Client Setup (15 minutes per machine)
- [ ] Build release version
- [ ] Run windeployqt
- [ ] Create installer
- [ ] Test installer
- [ ] Configure config.ini
- [ ] Install on client machines

### Go Live (1 hour)
- [ ] Final testing
- [ ] Backup everything
- [ ] Train users
- [ ] Monitor for issues
- [ ] Celebrate! 🎉

---

## Contact and Support

**For Technical Issues:**
- Check logs: `C:\xampp\apache\logs\error.log`
- Check application output in Qt Creator
- Review BUG_REPORT.md

**For Updates:**
- Check FIXES_APPLIED.md for latest changes
- Review GitHub issues (if using version control)

---

**Deployment Guide Version:** 1.0
**Last Updated:** 2025-10-18
**Prepared By:** Claude Code Analysis
**Application:** WITS Library Management System
