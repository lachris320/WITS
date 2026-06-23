# Adding RFID Enable/Disable Checkbox to Admin UI

## 📋 Quick Setup Guide

The RFID feature is now completely optional and can be toggled on/off from the admin panel. Follow these steps to add the checkbox to your UI:

## 🎨 Adding the Checkbox in Qt Designer

### Step 1: Open adminwindow.ui in Qt Designer

1. In Qt Creator, double-click `adminwindow.ui` in the project tree
2. This opens the Qt Designer visual editor

### Step 2: Navigate to General Page

1. In the widget tree (left side), find `stackedWidget`
2. Expand it and select `generalPage`
3. You should see the general settings form

### Step 3: Add the RFID Checkbox

1. From the **Widget Box** (left sidebar), drag a **QCheckBox** onto the generalPage
2. Place it near the `guestLoginCheckBox` (if you have one)
3. **Right-click** the new checkbox → **Change objectName...**
4. Set the object name to: **`rfidEnabledCheckBox`** (exactly this name)
5. Double-click the checkbox text and change it to: **"Enable RFID Feature"**

### Step 4: Add a Label (Optional but Recommended)

1. Drag a **QLabel** above the checkbox
2. Set its text to: **"RFID Settings:"**
3. Make it bold if desired

### Step 5: Save and Build

1. Save the UI file (Ctrl+S)
2. Rebuild the project

## 📝 Alternative: Manual Layout

If you're comfortable with layouts, you can organize it like this:

```
┌─────────────────────────────────────┐
│ RFID Settings:                      │
│ ☐ Enable RFID Feature               │
│                                     │
│ (Requires RFID hardware. Allows     │
│  students to scan cards for login)  │
└─────────────────────────────────────┘
```

## ✅ Verification

After adding the checkbox:

1. Run the application
2. Login as admin
3. Go to General page
4. You should see the "Enable RFID Feature" checkbox
5. Check it and click "Apply Changes"
6. Check Application Output - you should see:
   ```
   RFID feature enabled dynamically
   RFID Reader connected on port: COMX
   ```
   OR
   ```
   No RFID reader found (this is OK if RFID feature not purchased)
   ```

## 🔧 Testing the Feature

### With RFID Hardware:
1. Check the "Enable RFID Feature" box
2. Click "Apply Changes"
3. Connect RFID reader via USB
4. Scan a registered card
5. Student should auto-login

### Without RFID Hardware:
1. Leave the "Enable RFID Feature" box unchecked
2. Click "Apply Changes"
3. App works normally without RFID
4. No error messages about missing hardware

## 💰 Business Model Integration

### For Schools WITH RFID Package:
- Enable the checkbox during setup/onboarding
- Provide RFID hardware (reader + cards)
- Train staff on card registration
- Premium pricing tier

### For Schools WITHOUT RFID Package:
- Leave checkbox unchecked (default)
- App works with manual ID entry
- No RFID hardware needed
- Standard pricing tier

### Upgrade Path:
1. School starts with basic package (no RFID)
2. Later decides to upgrade
3. Purchase RFID hardware from you
4. Admin checks "Enable RFID Feature"
5. Registers student cards
6. Feature activates immediately!

## 🎯 Default State

The checkbox is **unchecked by default** (RFID disabled), making the app work out-of-the-box without any hardware requirements.

---

## ⚙️ Technical Details

**Object Name:** `rfidEnabledCheckBox` (must be exact)
**Location:** `generalPage` in `adminwindow.ui`
**Setting Key:** `rfid/enabled` in QSettings
**Default Value:** `false` (disabled)

The code uses `findChild<QCheckBox*>("rfidEnabledCheckBox")` to locate this checkbox, so the name must match exactly.

## 📸 Screenshot Reference

Your General page should look something like:

```
┌────────────────────────────────────────┐
│ General Settings                       │
├────────────────────────────────────────┤
│                                        │
│ School Name: [________________]        │
│ Address:     [________________]        │
│                                        │
│ Font Settings: [Arial    ▼] [12]      │
│                                        │
│ Library Hours:                         │
│   Opens:  [8  ] AM                     │
│   Closes: [10 ] PM                     │
│                                        │
│ Features:                              │
│ ☐ Enable Guest Login                  │
│ ☐ Enable RFID Feature          ← NEW! │
│                                        │
│ [Apply Changes]                        │
└────────────────────────────────────────┘
```

---

**Questions?** Check the `RFID_SETUP_GUIDE.md` for hardware setup and troubleshooting.
