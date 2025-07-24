# Google Drive OAuth2 Authentication Fix

*Resolving "The specified device is invalid" error*
*Date: 2025-07-23*

## Issue Description

User found Google Drive integration but encountered **"The specified device is invalid"** error when entering username/password. This indicates LibreOffice is using **traditional authentication** instead of **OAuth2** for Google Drive.

## Root Cause

Google Drive **requires OAuth2 authentication**, not username/password. The error occurs when LibreOffice presents a generic CMIS login dialog instead of triggering the OAuth2 flow.

## ✅ Correct OAuth2 Flow (Expected)

Based on source code analysis (`ucb/source/ucp/cmis/auth_provider.cxx`):

1. **Select Google Drive** → Should automatically detect OAuth2 requirement
2. **OAuth Dialog Appears** → LibreOffice presents authorization dialog
3. **Browser Opens** → Google consent screen launches
4. **User Authorizes** → Grants LibreOffice access to Drive
5. **Code Entry** → User copies authorization code back to LibreOffice
6. **Token Exchange** → LibreOffice exchanges code for access tokens
7. **Drive Access** → File browser shows Google Drive contents

## 🔧 Step-by-Step Fix

### **Step 1: Ensure Clean Restart**
✅ **DONE** - LibreOffice restarted with OAuth credentials

### **Step 2: Navigate to Correct Dialog**

1. **Open LibreOffice** (should be running now)
2. **Go to: File → Open Remote Files**
3. **Look for**:
   - "Add Service" or "Add Place" button
   - Service/Server dropdown list
   - "Google Drive" option

### **Step 3: Select Google Drive (Proper Method)**

**🎯 CRITICAL**: When you see Google Drive option:

- ✅ **DO**: Select "Google Drive" from dropdown
- ✅ **DO**: Leave username field **EMPTY** initially
- ✅ **DO**: Click "Connect" or "Next" without entering password
- ❌ **DON'T**: Enter georgejeffreys777@gmail.com in username field
- ❌ **DON'T**: Enter your Google password

### **Step 4: Expected OAuth2 Flow**

After selecting Google Drive, you should see **one of these**:

**Option A: Automatic Browser Launch**
- Browser opens to `accounts.google.com`
- Google OAuth consent screen appears
- You authorize LibreOffice access
- Browser shows authorization code
- Copy code back to LibreOffice dialog

**Option B: Manual Authorization Code Dialog**
- LibreOffice shows dialog: "Authorization Code Required"
- Dialog contains URL to visit
- You manually open URL in browser
- Complete OAuth flow and copy code back

## 🚨 Troubleshooting Alternative Paths

### **If Still Getting Username/Password Dialog:**

1. **Try Different UI Path**:
   - File → Save As → Look for "Remote" option
   - Start Center → "Open Remote Files"
   - Tools → Options → LibreOffice → Look for remote services

2. **Check Service Configuration**:
   - Look for "Add Network Place" or similar
   - Find "Server Type" dropdown with "Google Drive"
   - Ensure you're selecting "Google Drive" not "Other CMIS"

3. **Manual URL Method** (Advanced):
   - If you can enter a URL manually, try: `vnd.libreoffice.cmis://`
   - This should trigger CMIS URL parsing

### **If OAuth2 Dialog Appears But Fails:**

1. **Check Google Account Settings**:
   - Ensure 2FA is not blocking OAuth apps
   - Check Google Account → Security → Third-party app access

2. **Browser Issues**:
   - Try different browser (Safari, Chrome, Firefox)
   - Clear browser cookies for Google
   - Try incognito/private browsing mode

## 📊 Expected Behavior vs Reality

| Step | Expected | What User Experienced | Status |
|------|----------|----------------------|--------|
| Find Google Drive | ✅ Located in UI | ✅ **SUCCESS** | ✅ |
| Authentication Method | OAuth2 flow | Username/password dialog | ❌ |
| Browser Launch | Auto-opens OAuth URL | Not triggered | ❌ |
| Authorization | Google consent screen | "Device invalid" error | ❌ |

## 🎯 Next Actions

1. **Navigate back to Google Drive option**
2. **Try connecting WITHOUT entering username/password**
3. **Look for OAuth/authorization dialogs**
4. **Document exact UI path and dialog sequence**

## 🔍 Debug Information

**OAuth Configuration Status:**
- Client ID: ✅ Configured (`652946642777-...`)
- Client Secret: ✅ Configured (`GOCSPX-...`)
- LibreOffice Build: ✅ OAuth2 support enabled
- Expected Result: ✅ OAuth2 flow should trigger automatically

The integration is properly configured - we just need to find the right UI path that triggers OAuth2 instead of traditional authentication.
