# OAuth Overhaul Implementation Brief for Claude Agent

## MISSION STATEMENT
Design and implement a comprehensive OAuth2 modernization for LibreOffice that solves current token expiry crashes, enables runtime configuration, and provides enterprise-grade authentication while preserving the existing UCB/CMIS architecture.

## CRITICAL SUCCESS CRITERIA
1. **ZERO BREAKING CHANGES** - Existing Google Drive integration must continue working
2. **CRASH ELIMINATION** - No more token expiry crashes or hangs
3. **RUNTIME CONFIGURATION** - Move from compile-time to user-configurable OAuth
4. **PRODUCTION READY** - Enterprise-grade reliability and security
5. **CROSS-PLATFORM** - Works on Windows, macOS, Linux (including ARM64)

## CURRENT ARCHITECTURE ANALYSIS

### LibreOffice Component Hierarchy
```
Applications (Writer, Calc, etc.)
    ↓
UCB (Universal Content Broker)
    ↓
CMIS Provider (ucb/source/ucp/cmis/)
    ↓
AuthProvider (OAuth2 handling)
    ↓
libcmis (External library)
    ↓
libcurl (HTTP client)
```

### Current OAuth Implementation Location
- **Primary**: `ucb/source/ucp/cmis/auth_provider.cxx/hxx`
- **Configuration**: `config_host/config_oauth2.h.in`
- **Build Integration**: `configure.ac` (--with-gdrive-client-id)
- **UI Components**: `fpicker/source/office/RemoteFilesDialog.cxx`

### Current OAuth Flow (What Works)
1. User selects Google Drive in file dialog
2. AuthProvider detects OAuth requirement
3. Browser launches to Google consent screen
4. User manually copies authorization code
5. LibreOffice exchanges code for tokens
6. Tokens stored in platform keychain
7. Session cached for reuse

### Current OAuth Problems (What's Broken)
1. **Token Expiry Crashes**: When tokens expire mid-download, LibreOffice crashes
2. **Poor UX**: Manual authorization code entry is cumbersome
3. **Compile-time Limitation**: OAuth credentials baked into build
4. **Single Account**: No multi-account support
5. **Poor Error Recovery**: Failed refresh attempts cause permanent failures
6. **No Background Refresh**: Tokens expire without proactive renewal

## TECHNICAL CONSTRAINTS & REQUIREMENTS

### Must Preserve
- **UCB Architecture**: Don't break the Universal Content Broker pattern
- **CMIS Compatibility**: Existing CMIS integration for OneDrive, Alfresco
- **UNO Component Model**: Follow LibreOffice's component architecture
- **Cross-Platform**: Windows, macOS, Linux support
- **Existing APIs**: Don't break existing UCB interfaces

### Must Implement
- **Runtime OAuth Config**: Users configure OAuth without rebuilding
- **Automatic Token Refresh**: Background token renewal before expiry
- **PKCE Support**: Modern OAuth2 security standard
- **Device Flow**: Better for headless/server environments
- **Enhanced Error Handling**: Graceful failure with user-friendly messages
- **Multi-Provider Support**: Extensible for future cloud providers

### Security Requirements
- **Secure Storage**: Platform keychain integration (existing)
- **Token Encryption**: Encrypt stored tokens
- **Scope Validation**: Validate OAuth scopes
- **HTTPS Only**: All OAuth communications over TLS
- **Credential Rotation**: Support for credential updates

## PROPOSED ARCHITECTURE CHANGES

### New Components to Design
1. **Enhanced OAuth Manager** (UNO Service)
   - Centralized OAuth2 token management
   - Provider-agnostic interface
   - Background token refresh
   - Multi-account support

2. **Runtime Configuration System**
   - UI for OAuth app setup
   - Secure credential storage
   - Provider templates (Google, Microsoft, etc.)
   - Enterprise configuration support

3. **Modern OAuth Flows**
   - PKCE implementation
   - Device flow support
   - Automatic browser integration
   - Error recovery mechanisms

4. **Enhanced Error Handling**
   - User-friendly error messages
   - Automatic retry logic
   - Graceful degradation
   - Debug logging

### Integration Points
- **UCB Provider Registration**: How new OAuth manager integrates with UCB
- **CMIS Compatibility**: Maintain existing CMIS provider interfaces
- **UI Integration**: Settings dialogs, file picker enhancements
- **Build System**: Remove compile-time OAuth dependencies

## IMPLEMENTATION APPROACH REQUIREMENTS

### Phase 1: Foundation (Weeks 1-2)
- Design UNO interfaces for new OAuth manager
- Create backwards-compatible AuthProvider wrapper
- Implement basic runtime configuration storage
- Add comprehensive logging and error handling

### Phase 2: Core Implementation (Weeks 3-4)
- Implement enhanced token manager with refresh logic
- Add PKCE and device flow support
- Create configuration UI components
- Integrate with existing CMIS provider

### Phase 3: Integration & Testing (Weeks 5-6)
- Comprehensive testing across platforms
- Migration path for existing configurations
- Documentation and user guides
- Performance optimization

### Phase 4: Advanced Features (Weeks 7-8)
- Multi-account support
- Enterprise configuration features
- Advanced error recovery
- Admin/IT configuration options

## RISK ANALYSIS REQUIRED

### Technical Risks
- **Breaking UCB compatibility**: How to avoid disrupting existing file operations
- **Threading Issues**: OAuth refresh in background vs. LibreOffice's Solar Mutex
- **Platform Differences**: OAuth flows vary across Windows/macOS/Linux
- **Third-party Dependencies**: libcmis version compatibility

### User Experience Risks
- **Migration Complexity**: Moving users from current to new OAuth system
- **Configuration Complexity**: Making OAuth setup user-friendly
- **Error Communication**: Ensuring users understand OAuth failures
- **Performance Impact**: Ensuring OAuth operations don't slow down file access

### Security Risks
- **Credential Exposure**: Runtime storage of OAuth credentials
- **Token Leakage**: Secure handling of access/refresh tokens
- **Attack Vectors**: CSRF, token theft, etc.
- **Enterprise Compliance**: Meeting corporate security requirements

## SUCCESS VALIDATION CRITERIA

### Functional Tests
- [ ] Existing Google Drive integration continues working unchanged
- [ ] New OAuth configuration UI is intuitive and error-free
- [ ] Token refresh happens automatically before expiry
- [ ] Multiple Google accounts can be configured simultaneously
- [ ] PKCE and device flows work on all platforms
- [ ] Error recovery gracefully handles network failures

### Performance Tests
- [ ] OAuth operations don't block LibreOffice UI
- [ ] File operations are no slower than current implementation
- [ ] Memory usage doesn't increase significantly
- [ ] Background token refresh is unobtrusive

### Security Tests
- [ ] Tokens are stored securely and encrypted
- [ ] OAuth flows follow security best practices
- [ ] No credential leakage in logs or crash dumps
- [ ] Enterprise security requirements are met

### Platform Tests
- [ ] Works identically on Windows, macOS, Linux
- [ ] ARM64 and x86_64 architectures supported
- [ ] Various LibreOffice versions (dev builds, stable releases)
- [ ] Integration with system browsers and OAuth flows

## DELIVERABLES EXPECTED

1. **Detailed Technical Design Document**
   - UNO interface definitions
   - Class hierarchy and relationships
   - Sequence diagrams for OAuth flows
   - Database schema for configuration storage
   - Error handling strategies

2. **Implementation Plan**
   - Specific files to modify/create
   - Build system changes required
   - Migration strategy for existing users
   - Testing approach and test cases

3. **Risk Mitigation Strategy**
   - Identified risks with likelihood/impact
   - Specific mitigation approaches
   - Rollback plans if issues arise
   - Compatibility testing approach

4. **Prototype Validation Plan**
   - Minimal viable implementation for testing
   - Specific test scenarios to validate
   - Success criteria for each phase
   - Go/no-go decision points

## CONTEXT FILES PROVIDED
- Current AuthProvider implementation (`ucb/source/ucp/cmis/auth_provider.cxx`)
- CMIS Content implementation with token refresh issues
- OAuth configuration system (`config_host/config_oauth2.h.in`)
- Roadmap documents with enhancement plans
- Crash analysis showing token expiry problems
- Architecture analysis of LibreOffice Core

## YOUR TASK
Analyze the provided context and create a comprehensive, actionable implementation plan for the OAuth overhaul that addresses all the requirements above while minimizing risk to the existing LibreOffice Google Drive integration.

Focus on:
1. **Safety First**: How to implement without breaking existing functionality
2. **Incremental Approach**: Phase-by-phase implementation with validation
3. **Architecture Integrity**: Preserving LibreOffice's design patterns
4. **User Experience**: Making OAuth configuration simple and reliable
5. **Enterprise Readiness**: Meeting production-grade requirements

Provide specific technical recommendations, code structure proposals, and a detailed roadmap for implementation.
