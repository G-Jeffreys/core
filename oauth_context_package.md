# OAuth Overhaul Context Package

## KEY TECHNICAL FILES

### 1. Current AuthProvider Implementation (`ucb/source/ucp/cmis/auth_provider.cxx`)

```cpp
/* Current OAuth Implementation - Manual Authorization Code Flow */

char* AuthProvider::copyWebAuthCodeFallback( const char* url,
        const char* /*username*/,
        const char* /*password*/ )
{
    OUString url_oustr( url, strlen( url ), RTL_TEXTENCODING_UTF8 );
    const css::uno::Reference<
        css::ucb::XCommandEnvironment> xEnv = getXEnv( );

    if ( xEnv.is() )
    {
        uno::Reference< task::XInteractionHandler > xIH
            = xEnv->getInteractionHandler();

        if ( xIH.is() )
        {
            rtl::Reference< ucbhelper::AuthenticationFallbackRequest > xRequest
                = new ucbhelper::AuthenticationFallbackRequest (
                        u"Open the following link in your browser and "
                        "paste the code from the URL you have been redirected to in the "
                        "box below. For example:\n"
                        "http://localhost/LibreOffice?code=YOUR_CODE"_ustr,
                        url_oustr );

            xIH->handle( xRequest );
            // ... manual code entry handling
        }
    }
    return strdup( "" );
}

// Token Storage using Platform Keychain
std::string AuthProvider::getRefreshToken(std::string& rUsername)
{
    // Uses task::PasswordContainer for secure storage
    const uno::Reference<uno::XComponentContext>& xContext
        = ::comphelper::getProcessComponentContext();
    uno::Reference<task::XPasswordContainer2> xMasterPasswd
        = task::PasswordContainer::create(xContext);

    // Platform keychain integration (macOS Keychain, Windows Credential Store, etc.)
    task::UrlRecord aRec = xMasterPasswd->findForName(m_sBindingUrl, STD_TO_OUSTR(rUsername), xIH);
    // ...
}
```

**ANALYSIS**: Current implementation requires manual authorization code entry, stores tokens securely, but lacks automatic refresh.

### 2. Current OAuth Configuration (`config_host/config_oauth2.h`)

```cpp
/* Compile-time OAuth Configuration */
#define GDRIVE_BASE_URL "https://www.googleapis.com/drive/v3"
#define GDRIVE_CLIENT_ID "652946642777-brc4vu9al6fhvl2k8s5toh28c3les104.apps.googleusercontent.com"
#define GDRIVE_CLIENT_SECRET "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"
#define GDRIVE_AUTH_URL "https://accounts.google.com/o/oauth2/v2/auth"
#define GDRIVE_TOKEN_URL "https://oauth2.googleapis.com/token"
#define GDRIVE_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"
#define GDRIVE_SCOPE "https://www.googleapis.com/auth/drive"

/* OneDrive (Currently Empty) */
#define ONEDRIVE_CLIENT_ID ""
#define ONEDRIVE_CLIENT_SECRET ""

/* Alfresco Cloud (Currently Empty) */
#define ALFRESCO_CLOUD_CLIENT_ID ""
#define ALFRESCO_CLOUD_CLIENT_SECRET ""
```

**ANALYSIS**: OAuth credentials are baked into the build, limiting deployment flexibility and requiring recompilation for changes.

### 3. Token Refresh Problem (`ucb/source/ucp/cmis/cmis_content.cxx`)

```cpp
// Current Issue: Token expiry causes crashes
if (m_aURL.getBindingUrl() == GDRIVE_BASE_URL || m_aURL.getBindingUrl() == ONEDRIVE_BASE_URL)
{
    // Force session refresh for OAuth providers to ensure tokens are valid
    OUString sSessionId = m_aURL.getBindingUrl( ) + m_aURL.getRepositoryId( );
    m_pProvider->registerSession(sSessionId, m_aURL.getUsername( ), nullptr); // Clear cached session
    m_pSession = nullptr; // Force session recreation

    // This will create a fresh session with valid tokens
    if (!getSession(xEnv))
    {
        SAL_WARN("ucb.ucp.cmis", "Failed to refresh OAuth session before download");
        ucbhelper::cancelCommandExecution(
            ucb::IOErrorCode_ACCESS_DENIED,
            uno::Sequence< uno::Any >( 0 ),
            xEnv,
            u"Google Drive authentication expired - please reconnect to Google Drive"_ustr);
        return false;
    }
}
```

**ANALYSIS**: Manual session clearing is a workaround. Need automatic token refresh before expiry.

## ARCHITECTURAL ANALYSIS

### Current UCB/CMIS Architecture
```
LibreOffice Apps
    ↓ (UCB Interface)
Universal Content Broker (UCB)
    ↓ (Content Provider Registration)
CMIS ContentProvider
    ↓ (Authentication)
AuthProvider (OAuth2)
    ↓ (HTTP Requests)
libcmis → libcurl
```

### Current OAuth Flow Problems
1. **Manual Code Entry**: User must copy/paste authorization codes
2. **No Background Refresh**: Tokens expire causing crashes
3. **Compile-time Config**: Can't change OAuth settings without rebuilding
4. **Single Account**: No multi-account support
5. **Poor Error Recovery**: Failures require full re-authentication

### Token Expiry Crash Pattern
```
1. User opens Google Drive file
2. OAuth token has expired (typical: 1 hour)
3. CMIS attempts download with expired token
4. Google returns HTML error page instead of file content
5. LibreOffice tries to parse HTML as binary document
6. Assertion failure or infinite loop → CRASH
```

## ROADMAP ANALYSIS

### Phase 1 Goals (From existing roadmap):
- **Runtime OAuth Configuration** - Remove compile-time dependency
- **Enhanced Token Manager** - Automatic refresh, better error handling
- **PKCE Support** - Modern OAuth2 security
- **Backward Compatibility** - Existing Google Drive connections continue working

### Phase 2 Goals:
- **Multi-Account Support** - Multiple Google accounts
- **Device Flow** - Better for headless environments
- **Enhanced UI** - User-friendly OAuth setup
- **Enterprise Features** - Admin configuration, SSO integration

### Success Criteria:
- ✅ Zero breaking changes to existing Google Drive integration
- ✅ Elimination of token expiry crashes
- ✅ User-configurable OAuth without rebuilding LibreOffice
- ✅ Cross-platform compatibility (Windows, macOS, Linux)
- ✅ Enterprise-grade security and reliability

## IDENTIFIED PROBLEMS TO SOLVE

### 1. Token Management Issues
- **Problem**: Tokens expire without warning, causing crashes
- **Current Fix**: Manual session clearing (workaround)
- **Needed**: Automatic token refresh 5-10 minutes before expiry

### 2. Configuration Inflexibility
- **Problem**: OAuth credentials compiled into build
- **Current**: `#define GDRIVE_CLIENT_ID "..."`
- **Needed**: Runtime configuration with UI for setup

### 3. Poor User Experience
- **Problem**: Manual authorization code copy/paste
- **Current**: User opens browser, copies code manually
- **Needed**: Automatic browser integration with redirect handling

### 4. Limited Enterprise Support
- **Problem**: Single OAuth app for all users
- **Current**: Shared LibreOffice OAuth credentials
- **Needed**: Support for enterprise OAuth apps

### 5. Error Handling Gaps
- **Problem**: OAuth failures cause crashes or hangs
- **Current**: Assertion failures, infinite loops
- **Needed**: Graceful error handling with user-friendly messages

## TECHNICAL CONSTRAINTS

### Must Preserve:
1. **UCB Architecture** - Don't break existing file access patterns
2. **CMIS Compatibility** - OneDrive, Alfresco integration continues working
3. **Platform Support** - Windows, macOS, Linux (including ARM64)
4. **UNO Component Model** - Follow LibreOffice's component architecture
5. **Existing APIs** - Don't break UCB interfaces

### LibreOffice-Specific Requirements:
1. **Solar Mutex** - LibreOffice's main thread synchronization
2. **UNO Services** - Component registration and lifecycle
3. **Cross-Platform UI** - VCL toolkit for dialogs
4. **Build System** - gbuild integration
5. **Internationalization** - Multi-language support

### Security Requirements:
1. **Platform Keychain** - Continue using existing secure storage
2. **Token Encryption** - Encrypt stored tokens
3. **HTTPS Only** - All OAuth communications over TLS
4. **Scope Validation** - Validate OAuth scopes
5. **Enterprise Compliance** - Meet corporate security standards

## IMPLEMENTATION SAFETY REQUIREMENTS

### Backward Compatibility Strategy:
1. **Gradual Migration** - Existing configurations continue working
2. **Fallback Support** - If new system fails, fall back to old system
3. **Feature Flags** - Enable new features incrementally
4. **Migration Tools** - Automatic migration of existing tokens

### Risk Mitigation:
1. **Comprehensive Testing** - All platforms, all OAuth providers
2. **Rollback Plan** - Ability to revert to current implementation
3. **Monitoring** - Extensive logging for troubleshooting
4. **User Communication** - Clear error messages and recovery instructions

## SUCCESS VALIDATION CHECKLIST

### Functional Requirements:
- [ ] Existing Google Drive integration works unchanged
- [ ] Runtime OAuth configuration UI works intuitively
- [ ] Automatic token refresh prevents expiry crashes
- [ ] PKCE and device flows work on all platforms
- [ ] Multi-account support for Google Drive
- [ ] Error recovery handles network failures gracefully

### Performance Requirements:
- [ ] OAuth operations don't block LibreOffice UI
- [ ] File operations are no slower than current implementation
- [ ] Memory usage doesn't increase significantly
- [ ] Background token refresh is unobtrusive

### Security Requirements:
- [ ] Tokens stored securely with encryption
- [ ] OAuth flows follow current security best practices
- [ ] No credential leakage in logs or crash dumps
- [ ] Enterprise security requirements met

### Platform Requirements:
- [ ] Identical functionality on Windows, macOS, Linux
- [ ] ARM64 and x86_64 architectures supported
- [ ] Integration with system browsers works reliably
- [ ] Platform-specific OAuth flows handled correctly

This context package provides everything needed to design and implement a comprehensive OAuth overhaul for LibreOffice that solves the token expiry crashes while maintaining full backward compatibility and enterprise-grade reliability.
