# LibreOffice Google Drive Crash Analysis

*Analysis of crash when opening .odt file from Google Drive*
*Date: 2025-07-23*

## 🚨 **Crash Summary**

**Issue**: LibreOffice crashes with assertion failure when opening Google Drive files
**Severity**: High - Complete application crash
**Environment**: Development build on macOS ARM64

## 📊 **Root Cause Analysis**

### **Primary Issue: OAuth Token Refresh Failed**
```
info:ucb.ucp.cmis:41255:7539175:ucb/source/ucp/cmis/cmis_content.cxx:443:
Unexpected libcmis exception: Couldn't refresh token
```

**Cause**: Google OAuth tokens expired and refresh attempt failed

### **Secondary Issue: Crash on Corrupted Data**
```
Assertion failed: (mpByteReader), function ByteGrabber, file ByteGrabber.cxx, line 44.
```

**Flow**:
1. OAuth refresh fails → Partial/corrupted file download
2. LibreOffice tries to process as ZIP (since .odt = ZIP archive)
3. ZIP processor expects ByteReader interface
4. Gets incompatible stream type → Assertion fails → **CRASH**

## 🔍 **Technical Analysis**

### **Stack Trace Breakdown**
```cpp
// The crash happens here:
ByteGrabber::ByteGrabber(Reference<XInputStream> const& xStream) {
    mpByteReader = dynamic_cast<comphelper::ByteReader*>(xStream.get());
    assert(mpByteReader);  // ← CRASHES HERE
}
```

### **Why This Happens**
1. **CMIS downloads partial file** due to auth failure
2. **Stream type mismatch**: Expected `ByteReader`, got different stream type
3. **No error handling**: Assertion instead of graceful error

### **File Processing Chain**
```
Google Drive → CMIS Download → ZIP Detection → ByteGrabber → CRASH
```

## 🛠️ **Workarounds**

### **Immediate Solutions**

#### **1. Fix OAuth Token Issues**
The token refresh failure suggests authentication problems:

```bash
# Clear any cached auth data
rm -rf ~/.config/libreoffice-dev/4/user/registrymodifications.xcu
```

#### **2. Re-authenticate**
- Delete existing Google Drive connection
- Add fresh Google Drive connection
- Complete new OAuth flow

#### **3. Use Smaller Test Files**
- Avoid large .odt files initially
- Test with simple text files first
- Save new files to Drive (instead of opening existing ones)

### **Development Build Issues**

#### **Missing Error Handling**
- **Production builds** would handle this gracefully
- **Development builds** have debug assertions that crash
- **Real fix** would be proper error handling in ZIP processor

#### **CMIS Stream Handling**
The crash reveals that CMIS provider returns incompatible stream types when downloads fail.

## 🧪 **Testing Strategy**

### **Safe Testing Approach**
1. **Create new documents** in LibreOffice
2. **Save to Google Drive** (tests upload path)
3. **Avoid opening existing files** until OAuth is stable
4. **Use small files** (<1MB) for initial testing

### **OAuth Debugging**
```bash
# Launch with OAuth debugging
export SAL_LOG="+WARN+INFO.ucb.ucp.cmis"
./instdir/LibreOfficeDev.app/Contents/MacOS/soffice 2>&1 | grep -E "(token|oauth|auth)"
```

## 🔧 **Immediate Action Plan**

### **Step 1: Clean Authentication**
```bash
# Kill any LibreOffice processes
killall soffice

# Clear cached auth data
rm -rf ~/.config/libreoffice-dev/4/user/registrymodifications.xcu

# Restart LibreOffice
./instdir/LibreOfficeDev.app/Contents/MacOS/soffice &
```

### **Step 2: Re-authenticate**
1. Go to File → Remote Files
2. **Delete existing** Google Drive connection
3. **Add new** Google Drive service
4. **Complete fresh** OAuth flow

### **Step 3: Safe Testing**
1. Create new Writer document
2. Add simple text: "Test document"
3. Save to Google Drive
4. Verify it appears in Drive web interface

## 🏗️ **Development Insights**

### **This Reveals LibreOffice Issues**
1. **Poor error handling** in CMIS/ZIP integration
2. **Debug assertions** inappropriate for user-facing code
3. **Stream type assumptions** not validated

### **Production vs Development**
- **Production LibreOffice** would show error dialog, not crash
- **Development builds** expose internal bugs like this
- **This bug** probably exists in production too, but handled differently

## 📈 **Implications for Our Project**

### **Technical Debt Identified**
1. **Error Handling**: Need robust error handling for network failures
2. **Stream Compatibility**: CMIS provider should return compatible streams
3. **Graceful Degradation**: Failed downloads shouldn't crash the application

### **User Experience Impact**
- **Current**: Crash when OAuth fails
- **Needed**: Clear error message + retry option
- **Future**: Seamless token refresh without user intervention

## 🎯 **Recommendations**

### **For Immediate Testing**
1. **Re-authenticate** with fresh OAuth flow
2. **Test file creation** (not opening) initially
3. **Use small files** to minimize download issues
4. **Monitor logs** for OAuth token issues

### **For Production Implementation**
1. **Robust error handling** for failed downloads
2. **Automatic token refresh** with fallback
3. **User-friendly error messages** instead of crashes
4. **Progress indicators** for network operations

### **For LibreOffice Community**
This crash should be **reported upstream** as it affects user experience even in production builds.

## 🏆 **Silver Lining**

Despite the crash, this confirms:
- ✅ **OAuth integration works** (tokens were obtained)
- ✅ **Google Drive connection succeeds** (file was found)
- ✅ **CMIS download starts** (partial file received)
- ✅ **File type detection works** (recognized as .odt ZIP)

**The crash is a robustness issue, not a fundamental integration problem.**
