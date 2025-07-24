# Phase 2: Integration & Automatic Token Refresh

**Duration**: 2 weeks
**Risk Level**: Medium
**Dependencies**: Phase 1 complete

## Objectives
- Integrate OAuth2 services with existing CMIS provider
- Implement automatic token refresh before expiry
- Add background token refresh timer
- Enhance error handling and recovery
- Eliminate token expiry crashes

## Files to Modify

### Enhanced CMIS Integration
```
ucb/source/ucp/cmis/cmis_content.cxx             (Token refresh integration)
ucb/source/ucp/cmis/cmis_provider.cxx            (OAuth2 service integration)
ucb/source/ucp/cmis/auth_provider.cxx            (Enhanced with OAuth2)
```

### New Background Services
```
ucb/source/ucp/oauth2/token_refresh_timer.cxx/hxx    (Background refresh)
ucb/source/ucp/oauth2/oauth2_error_handler.cxx/hxx   (Error recovery)
ucb/source/ucp/oauth2/network_detector.cxx/hxx       (Network availability)
```

### Enhanced UI Components
```
fpicker/source/office/RemoteFilesDialog.cxx          (OAuth2 configuration UI)
```

## Key Implementation Tasks

### Task 1: CMIS Provider Integration
- [ ] Modify CMIS content to use OAuth2 service
- [ ] Replace direct token access with OAuth2 service calls
- [ ] Add automatic token refresh before downloads
- [ ] Integrate enhanced error handling
- [ ] Maintain session compatibility

#### Critical Integration Points

**Before File Downloads:**
```cpp
// ucb/source/ucp/cmis/cmis_content.cxx - feedSink method
bool Content::feedSink(const uno::Reference<uno::XInterface>& xSink,
                      const uno::Reference<ucb::XCommandEnvironment>& xEnv)
{
    // CRITICAL: Check token validity before download
    if (m_aURL.getBindingUrl() == GDRIVE_BASE_URL ||
        m_aURL.getBindingUrl() == ONEDRIVE_BASE_URL)
    {
        // Get OAuth2 service
        uno::Reference<ucb::XOAuth2Service> xOAuth2Service =
            getOAuth2Service(m_xContext);

        // Ensure valid token (auto-refresh if needed)
        OUString sValidToken = xOAuth2Service->getValidAccessToken(
            m_aURL.getBindingUrl(),
            m_aURL.getUsername(),
            xEnv);

        if (sValidToken.isEmpty())
        {
            // Handle authentication failure gracefully
            handleOAuth2AuthenticationFailure(xEnv);
            return false;
        }

        // Update session with fresh token
        updateSessionWithToken(sValidToken, xEnv);
    }

    // Continue with existing download logic...
}
```

**Session Creation Enhancement:**
```cpp
// ucb/source/ucp/cmis/cmis_content.cxx - getSession method
libcmis::Session* Content::getSession(const uno::Reference<ucb::XCommandEnvironment>& xEnv)
{
    // Enhanced OAuth2 session creation
    if (m_aURL.getBindingUrl() == GDRIVE_BASE_URL ||
        m_aURL.getBindingUrl() == ONEDRIVE_BASE_URL)
    {
        // Use enhanced OAuth2 authentication
        EnhancedAuthProvider aAuthProvider(xEnv, m_xIdentifier->getContentIdentifier(),
                                         m_aURL.getBindingUrl());

        // Get valid access token (with automatic refresh)
        std::string accessToken = aAuthProvider.getValidAccessToken(
            OUSTR_TO_STDSTR(m_aURL.getUsername()));

        if (accessToken.empty())
        {
            // Trigger full re-authentication
            return performFullOAuth2Authentication(xEnv);
        }

        // Create session with valid token
        return createSessionWithToken(accessToken, xEnv);
    }

    // Existing logic for non-OAuth providers
    // ...
}
```

### Task 2: Background Token Refresh System

#### Token Refresh Timer Implementation
```cpp
// ucb/source/ucp/oauth2/token_refresh_timer.cxx
class TokenRefreshTimer : public salhelper::Timer
{
private:
    uno::Reference<ucb::XOAuth2Service> m_xOAuth2Service;
    uno::Reference<ucb::XTokenManager> m_xTokenManager;

public:
    TokenRefreshTimer(const uno::Reference<ucb::XOAuth2Service>& xOAuth2Service,
                     const uno::Reference<ucb::XTokenManager>& xTokenManager);

    void SAL_CALL onShot() override;

private:
    void refreshExpiringTokens();
    void handleRefreshFailure(const OUString& sProviderUrl,
                             const OUString& sUsername,
                             const std::exception& error);
};

void TokenRefreshTimer::refreshExpiringTokens()
{
    // Check all stored tokens for expiry (5-minute margin)
    const uno::Sequence<OUString> aProviders =
        m_xTokenManager->getConfiguredProviders();

    for (const auto& sProviderUrl : aProviders)
    {
        const uno::Sequence<OUString> aAccounts =
            m_xTokenManager->getStoredAccounts(sProviderUrl);

        for (const auto& sUsername : aAccounts)
        {
            if (m_xTokenManager->isTokenExpired(sProviderUrl, sUsername, 5))
            {
                try
                {
                    // Perform background refresh
                    m_xOAuth2Service->refreshTokens(sProviderUrl, sUsername, nullptr);

                    SAL_INFO("ucb.ucp.oauth2",
                            "Background token refresh successful: " << sProviderUrl
                            << " / " << sUsername);
                }
                catch (const uno::Exception& e)
                {
                    SAL_WARN("ucb.ucp.oauth2",
                            "Background token refresh failed: " << e.Message);
                    handleRefreshFailure(sProviderUrl, sUsername, e);
                }
            }
        }
    }
}
```

#### Timer Integration with OAuth2 Service
```cpp
// ucb/source/ucp/oauth2/oauth2_service.cxx
class OAuth2Service : public cppu::WeakImplHelper<ucb::XOAuth2Service>
{
private:
    rtl::Reference<TokenRefreshTimer> m_pRefreshTimer;

public:
    OAuth2Service(const uno::Reference<uno::XComponentContext>& xContext);

    void startBackgroundRefresh();
    void stopBackgroundRefresh();
};

OAuth2Service::OAuth2Service(const uno::Reference<uno::XComponentContext>& xContext)
    : m_xContext(xContext)
{
    // Initialize timer for background token refresh (every 2 minutes)
    m_pRefreshTimer = new TokenRefreshTimer(this, m_xTokenManager);
    m_pRefreshTimer->start(std::chrono::minutes(2));
}
```

### Task 3: Enhanced Error Handling & Recovery

#### OAuth2 Error Handler
```cpp
// ucb/source/ucp/oauth2/oauth2_error_handler.cxx
class OAuth2ErrorHandler
{
public:
    static void handleTokenExpiry(const OUString& sProviderUrl,
                                 const OUString& sUsername,
                                 const uno::Reference<ucb::XCommandEnvironment>& xEnv);

    static void handleNetworkError(const std::exception& error,
                                  const uno::Reference<ucb::XCommandEnvironment>& xEnv);

    static void handleAuthenticationFailure(const OUString& sProviderUrl,
                                           const OUString& sUsername,
                                           const uno::Reference<ucb::XCommandEnvironment>& xEnv);

    static bool shouldRetryAuthentication(sal_Int32 nConsecutiveFailures);

    static void showUserFriendlyError(const OUString& sErrorMessage,
                                     const uno::Reference<ucb::XCommandEnvironment>& xEnv);
};

void OAuth2ErrorHandler::handleTokenExpiry(const OUString& sProviderUrl,
                                          const OUString& sUsername,
                                          const uno::Reference<ucb::XCommandEnvironment>& xEnv)
{
    // Try automatic token refresh first
    try
    {
        uno::Reference<ucb::XOAuth2Service> xOAuth2Service =
            getOAuth2Service(comphelper::getProcessComponentContext());

        OUString sNewToken = xOAuth2Service->refreshTokens(sProviderUrl, sUsername, xEnv);

        if (!sNewToken.isEmpty())
        {
            SAL_INFO("ucb.ucp.oauth2", "Token refresh successful");
            return; // Success - no user intervention needed
        }
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Token refresh failed: " << e.Message);
    }

    // Auto-refresh failed - require user re-authentication
    showUserFriendlyError(
        u"Cloud storage authentication expired. Please reconnect in File → Remote Files."_ustr,
        xEnv);
}
```

### Task 4: Network Availability Detection

#### Smart Network Handling
```cpp
// ucb/source/ucp/oauth2/network_detector.cxx
class NetworkDetector
{
public:
    static bool isNetworkAvailable();
    static bool canReachProvider(const OUString& sProviderUrl);
    static void waitForNetworkRecovery(std::chrono::seconds timeout);

private:
    static bool performConnectivityCheck(const OUString& sUrl);
};

bool NetworkDetector::isNetworkAvailable()
{
    // Simple connectivity check to well-known endpoints
    const std::vector<OUString> aTestUrls = {
        u"https://www.google.com"_ustr,
        u"https://www.microsoft.com"_ustr,
        u"https://1.1.1.1"_ustr  // Cloudflare DNS
    };

    for (const auto& sUrl : aTestUrls)
    {
        if (performConnectivityCheck(sUrl))
            return true;
    }

    return false;
}
```

### Task 5: Enhanced UI Integration

#### OAuth2 Configuration Dialog
```cpp
// fpicker/source/office/RemoteFilesDialog.cxx - Enhanced configuration
void RemoteFilesDialog::addOAuth2Provider()
{
    // Launch OAuth2 provider configuration dialog
    uno::Reference<ucb::XOAuth2Configuration> xConfigManager =
        getOAuth2ConfigurationService();

    OAuth2ConfigurationDialog aDialog(this, xConfigManager);

    if (aDialog.Execute() == RET_OK)
    {
        // Provider configured successfully
        ucb::OAuth2ProviderConfig aConfig = aDialog.getConfiguration();
        xConfigManager->setProviderConfig(aConfig.sBaseUrl, aConfig);

        // Refresh provider list
        refreshProviderList();

        // Show success message
        showInfoMessage(u"Cloud storage provider configured successfully"_ustr);
    }
}
```

## Critical Integration Points

### Token Refresh Before Every Operation
```cpp
// Pattern for all CMIS operations that access cloud storage
bool Content::performCloudStorageOperation()
{
    if (isOAuth2Provider())
    {
        // 1. Check token validity
        if (!hasValidToken())
        {
            // 2. Attempt automatic refresh
            if (!refreshTokenIfPossible())
            {
                // 3. Require user re-authentication
                return triggerReAuthentication();
            }
        }

        // 4. Proceed with operation using valid token
        return performOperation();
    }

    // Non-OAuth providers use existing logic
    return performStandardOperation();
}
```

### Graceful Degradation Strategy
```cpp
enum class AuthenticationState
{
    AUTHENTICATED,      // Valid tokens available
    NEEDS_REFRESH,      // Tokens expired but refreshable
    NEEDS_REAUTH,       // Full re-authentication required
    NETWORK_ERROR,      // Network connectivity issues
    SERVICE_ERROR       // OAuth service unavailable
};

AuthenticationState checkAuthenticationState(const OUString& sProviderUrl,
                                           const OUString& sUsername)
{
    if (!NetworkDetector::isNetworkAvailable())
        return AuthenticationState::NETWORK_ERROR;

    if (!hasStoredTokens(sProviderUrl, sUsername))
        return AuthenticationState::NEEDS_REAUTH;

    if (isTokenExpired(sProviderUrl, sUsername))
        return AuthenticationState::NEEDS_REFRESH;

    return AuthenticationState::AUTHENTICATED;
}
```

## Testing Strategy for Phase 2

### Automated Tests
```cpp
// Test automatic token refresh
void testAutomaticTokenRefresh()
{
    // Set up expired token
    TokenManager tokenManager;
    tokenManager.storeTokens(GDRIVE_URL, "user@example.com",
                           "expired_token", "refresh_token",
                           DateTime::now() - TimeSpan::fromMinutes(10));

    // Attempt operation - should trigger refresh
    OAuth2Service oauth2Service;
    OUString sNewToken = oauth2Service.getValidAccessToken(GDRIVE_URL, "user@example.com", nullptr);

    // Verify new token obtained
    CPPUNIT_ASSERT(!sNewToken.isEmpty());
    CPPUNIT_ASSERT(sNewToken != "expired_token");
}

// Test background refresh timer
void testBackgroundRefresh()
{
    TokenRefreshTimer timer(m_xOAuth2Service, m_xTokenManager);

    // Set up token expiring in 4 minutes
    DateTime expiry = DateTime::now() + TimeSpan::fromMinutes(4);
    m_xTokenManager->storeTokens(GDRIVE_URL, "user@example.com",
                               "expiring_token", "refresh_token", expiry);

    // Trigger timer
    timer.onShot();

    // Verify token was refreshed
    OUString sRefreshedToken = m_xTokenManager->getAccessToken(GDRIVE_URL, "user@example.com");
    CPPUNIT_ASSERT(sRefreshedToken != "expiring_token");
}
```

### Integration Tests
- Download files with expired tokens (should auto-refresh)
- Background refresh timer functionality
- Network error recovery
- Multiple account token refresh
- Error handling and user messaging

### Performance Tests
- Token refresh performance impact
- Memory usage of background timer
- Response time with token validation
- Concurrent token refresh handling

## Success Criteria for Phase 2

✅ **Crash Elimination**
- No more token expiry crashes during file operations
- Graceful error handling for all OAuth failure scenarios
- Background token refresh prevents expiry issues

✅ **User Experience**
- Seamless file access without manual re-authentication
- Clear error messages when re-authentication needed
- Fast response times with token validation

✅ **Reliability**
- Background refresh works across all platforms
- Network error recovery functions correctly
- Multi-account support works reliably

✅ **Performance**
- No noticeable performance degradation
- Efficient token refresh (only when needed)
- Minimal memory and CPU overhead

## Risk Mitigation for Phase 2

**Risk**: Background refresh consuming resources
**Mitigation**: Efficient timer implementation, only refresh when needed

**Risk**: Network failures during refresh
**Mitigation**: Retry logic, graceful degradation, offline detection

**Risk**: Race conditions in token refresh
**Mitigation**: Mutex protection, atomic token updates

**Risk**: Breaking existing CMIS functionality
**Mitigation**: Gradual integration, extensive compatibility testing

**Risk**: User confusion during authentication failures
**Mitigation**: Clear error messages, guided recovery process

## Phase 2 Deliverables

1. ✅ Integrated automatic token refresh
2. ✅ Background refresh timer system
3. ✅ Enhanced error handling and recovery
4. ✅ Network awareness and retry logic
5. ✅ Comprehensive integration tests
6. ✅ Performance optimization
7. ✅ User experience improvements
