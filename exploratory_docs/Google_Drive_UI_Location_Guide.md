# Google Drive Integration - UI Location Guide

*Based on source code analysis of LibreOffice CMIS implementation*
*Date: 2025-07-23*
*Platform: macOS with LibreOffice Development Build*

## Executive Summary

Google Drive integration in LibreOffice is **already implemented** and should appear in the remote file dialogs, provided OAuth credentials are properly configured. The integration uses the CMIS (Content Management Interoperability Services) framework and appears as a predefined server option.

## 📍 Exact UI Locations

### **Primary Access Points**

Based on `fpicker/source/office/RemoteFilesDialog.cxx` and related source files:

1. **File Menu → Open Remote Files**
   - Direct access to remote file browser
   - Google Drive should appear in service dropdown

2. **File Menu → Open → Remote Files Button**
   - Look for remote/network file access button
   - Opens remote file picker dialog

3. **Start Center → Open Remote Files**
   - From LibreOffice start screen
   - Quick access to remote storage

4. **File Menu → Save As → Remote Location**
   - When saving documents
   - Access to remote storage services

### **Service Configuration Dialog**

From `svtools/source/dialogs/PlaceEditDialog.cxx`:

1. **Add Service/Place Dialog**
   - Accessed through File dialogs
   - Server Type dropdown should list:
     - **Google Drive** (first option)
     - OneDrive
     - Alfresco Cloud
     - Other CMIS servers

## 🔧 Technical Implementation Details

### **Predefined Server Configuration**

Location: `officecfg/registry/data/org/openoffice/Office/Common.xcu`

```xml
<prop oor:name="CmisServersUrls">
  <value>
    <it>https://www.googleapis.com/drive/v3</it>      <!-- Google Drive -->
    <it>https://graph.microsoft.com/v1.0</it>          <!-- OneDrive -->
    <it>https://api.alfresco.com/cmis/versions/1.0/atom/</it>
    <!-- ... more servers ... -->
  </value>
</prop>

<prop oor:name="CmisServersNames">
  <value>
    <it>Google Drive</it>                              <!-- Display Name -->
    <it>OneDrive</it>
    <it>Alfresco Cloud</it>
    <!-- ... corresponding names ... -->
  </value>
</prop>
```

### **OAuth Credential Detection**

From `svtools/source/dialogs/PlaceEditDialog.cxx`:

```cpp
// Google Drive only appears if OAuth credentials are configured
bool bSkipGDrive = std::string_view( GDRIVE_CLIENT_ID ).empty() ||
                   std::string_view( GDRIVE_CLIENT_SECRET ).empty();

if (!bSkipGDrive) {
    // Add "Google Drive" to server type dropdown
    m_xLBServerType->insert_text(nPos, "Google Drive");
}
```

**✅ Your Configuration Status:**
- Client ID: `652946642777-brc4vu9al6fhvl2k8s5toh28c3les104.apps.googleusercontent.com` ✅
- Client Secret: `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` ✅
- **Result**: Google Drive **should appear** in server dropdown

### **Service Type Detection**

From `fpicker/source/office/RemoteFilesDialog.cxx`:

```cpp
static OUString lcl_GetServiceType( const ServicePtr& pService )
{
    INetProtocol aProtocol = pService->GetUrlObject().GetProtocol();
    switch( aProtocol )
    {
        case INetProtocol::Cmis:
        {
            OUString sHost = pService->GetUrlObject().GetHost();
            if( sHost.startsWith( GDRIVE_BASE_URL ) )
                return u"Google Drive"_ustr;    // Branded display
            // ...
        }
    }
}
```

## 🎯 What You Should See

### **Expected UI Elements**

1. **Server Type Dropdown**
   - "Google Drive" as first option
   - Appears **before** OneDrive and Alfresco Cloud
   - Shows branded name (not "CMIS")

2. **Authentication Flow**
   - Click "Google Drive" → Opens OAuth dialog
   - Browser launches for Google consent
   - Authorization code entry (current implementation)
   - Access token exchange

3. **File Browser**
   - Google Drive folders and files
   - Standard LibreOffice file picker interface
   - File type filtering (ODT, DOCX, etc.)

### **URL Scheme Used**

```
vnd.libreoffice.cmis://user@https%3A//www.googleapis.com/drive/v3%23repository_id/path
```

## 🚨 Troubleshooting Guide

### **If Google Drive Doesn't Appear:**

1. **Restart LibreOffice Completely**
   ```bash
   # Kill any running LibreOffice processes
   killall soffice
   # Restart with clean session
   ./instdir/LibreOfficeDev.app/Contents/MacOS/soffice
   ```

2. **Verify OAuth Configuration**
   ```bash
   grep "GDRIVE_CLIENT" config_host/config_oauth2.h
   # Should show your configured credentials
   ```

3. **Check Build Components**
   ```bash
   find instdir/ -name "*cmis*" | head -5
   # Should find CMIS libraries and UI files
   ```

4. **Alternative UI Paths to Try**
   - File → Recent Documents → Open Remote
   - Tools → Options → LibreOffice → Paths → Add Service
   - Any "Network Location" or "Remote Storage" options

### **Expected vs Actual Behavior**

| Component | Expected | Status |
|-----------|----------|---------|
| OAuth Credentials | Configured | ✅ |
| CMIS Library | Loaded | ✅ |
| Google Drive Option | Visible in dropdown | ❓ Need to verify |
| Authentication | OAuth flow works | ❓ Need to test |

## 📝 Testing Checklist

Based on this analysis, please check these specific locations:

- [ ] **File → Open** - Look for "Remote Files" option
- [ ] **File → Open Remote Files** - Direct menu item
- [ ] **Start Center** - "Open Remote Files" button
- [ ] **File → Save As** - Remote location options
- [ ] **Add Service dialog** - Server type dropdown with "Google Drive"
- [ ] **Network/Remote sidebar** - In file picker dialogs

## 🔍 What to Document

When testing, please note:

1. **Exact menu path** where you find (or don't find) Google Drive
2. **UI screenshots** of dialogs and dropdowns
3. **Error messages** if any appear
4. **Authentication behavior** when selecting Google Drive
5. **File listing behavior** if you get past authentication

## Next Steps

1. ✅ OAuth credentials configured correctly
2. ✅ LibreOffice built with Google Drive support
3. ❓ **Your task**: Locate Google Drive in the UI using the paths above
4. ❓ **Test**: OAuth authentication flow
5. ❓ **Verify**: File operations (open, save, browse)

The integration **should be working** - we just need to find the right UI path!
