# Phase 1: Foundation & Core Services

**Duration**: 2 weeks
**Risk Level**: Low
**Dependencies**: None

## Objectives
- Create new UNO service interfaces
- Implement core OAuth2 manager service
- Establish runtime configuration system
- Add comprehensive logging and error handling
- Maintain 100% backward compatibility

## Files to Create

### New UNO Interface Definitions
```
offapi/com/sun/star/ucb/XOAuth2Service.idl
offapi/com/sun/star/ucb/XTokenManager.idl
offapi/com/sun/star/ucb/XOAuth2Configuration.idl
offapi/com/sun/star/ucb/OAuth2ProviderConfig.idl
```

### New Implementation Files
```
ucb/source/ucp/oauth2/                           (New directory)
├── oauth2_service.cxx/hxx                       (Main OAuth2 service)
├── token_manager.cxx/hxx                        (Token management)
├── config_manager.cxx/hxx                       (Configuration management)
├── flow_manager.cxx/hxx                         (OAuth flow handling)
├── pkce_helper.cxx/hxx                          (PKCE implementation)
├── device_flow.cxx/hxx                          (Device flow implementation)
├── oauth2_provider.cxx/hxx                      (UNO component provider)
└── ucpoauth2.component                          (Component registration)
```

### Enhanced Existing Files
```
ucb/source/ucp/cmis/auth_provider.cxx/hxx        (Enhanced AuthProvider)
ucb/source/ucp/cmis/cmis_content.cxx             (Token refresh integration)
```

## Build System Changes

### Add OAuth2 Module
```makefile
# ucb/source/ucp/oauth2/Library_ucpoauth2.mk
$(eval $(call gb_Library_Library,ucpoauth2))

$(eval $(call gb_Library_set_componentfile,ucpoauth2,ucb/source/ucp/oauth2/ucpoauth2))

$(eval $(call gb_Library_use_libraries,ucpoauth2,\
    comphelper \
    cppu \
    cppuhelper \
    sal \
    salhelper \
    tl \
    utl \
    vcl \
))

$(eval $(call gb_Library_add_exception_objects,ucpoauth2,\
    ucb/source/ucp/oauth2/oauth2_service \
    ucb/source/ucp/oauth2/token_manager \
    ucb/source/ucp/oauth2/config_manager \
    ucb/source/ucp/oauth2/flow_manager \
    ucb/source/ucp/oauth2/pkce_helper \
    ucb/source/ucp/oauth2/device_flow \
    ucb/source/ucp/oauth2/oauth2_provider \
))
```

### Update Build Dependencies
```makefile
# ucb/Library_ucpcmis1.mk (Enhanced CMIS provider)
$(eval $(call gb_Library_use_libraries,ucpcmis1,\
    ucpoauth2 \  # Add OAuth2 dependency
    # ... existing dependencies
))
```

## Configuration Schema Updates

### New Configuration Nodes
```xml
<!-- officecfg/registry/schema/org/openoffice/ucb/OAuth2.xcs -->
<component-schema xmlns="http://openoffice.org/2001/registry"
                  xmlns:xs="http://www.w3.org/2001/XMLSchema"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://openoffice.org/2001/registry
                  component-schema.xsd">

  <templates>
    <group oor:name="OAuth2Provider">
      <prop oor:name="DisplayName" oor:type="xs:string"/>
      <prop oor:name="BaseUrl" oor:type="xs:string"/>
      <prop oor:name="AuthUrl" oor:type="xs:string"/>
      <prop oor:name="TokenUrl" oor:type="xs:string"/>
      <prop oor:name="Scope" oor:type="xs:string"/>
      <prop oor:name="ClientId" oor:type="xs:string"/>
      <prop oor:name="ClientSecret" oor:type="xs:string"/>
      <prop oor:name="RedirectUri" oor:type="xs:string"/>
      <prop oor:name="UsePKCE" oor:type="xs:boolean"/>
      <prop oor:name="SupportDeviceFlow" oor:type="xs:boolean"/>
    </group>
  </templates>

  <component>
    <group oor:name="Providers">
      <set oor:name="OAuth2Providers" oor:node-type="OAuth2Provider"/>
    </group>
  </component>
</component-schema>
```

### Default Configuration Values
```xml
<!-- officecfg/registry/data/org/openoffice/ucb/OAuth2.xcu -->
<component-data xmlns="http://openoffice.org/2001/registry"
                xmlns:install="http://openoffice.org/2001/setup">
  <node oor:name="Providers">
    <node oor:name="OAuth2Providers">
      <!-- Google Drive default configuration -->
      <node oor:name="https://www.googleapis.com/drive/v3" oor:op="replace">
        <prop oor:name="DisplayName">
          <value>Google Drive</value>
        </prop>
        <prop oor:name="BaseUrl">
          <value>https://www.googleapis.com/drive/v3</value>
        </prop>
        <prop oor:name="AuthUrl">
          <value>https://accounts.google.com/o/oauth2/v2/auth</value>
        </prop>
        <prop oor:name="TokenUrl">
          <value>https://oauth2.googleapis.com/token</value>
        </prop>
        <prop oor:name="Scope">
          <value>https://www.googleapis.com/auth/drive</value>
        </prop>
        <prop oor:name="RedirectUri">
          <value>urn:ietf:wg:oauth:2.0:oob</value>
        </prop>
        <prop oor:name="UsePKCE">
          <value>true</value>
        </prop>
        <prop oor:name="SupportDeviceFlow">
          <value>false</value>
        </prop>
      </node>
    </node>
  </node>
</component-data>
```

## Key Implementation Tasks

### Task 1: UNO Service Infrastructure
- [ ] Create IDL interface definitions
- [ ] Generate C++ headers from IDL
- [ ] Implement basic service skeleton
- [ ] Register services with UNO runtime
- [ ] Add component registration

### Task 2: Core OAuth2 Service
- [ ] Implement XOAuth2Service interface
- [ ] Add authentication method with PKCE
- [ ] Implement token validation and refresh
- [ ] Add multi-account support
- [ ] Integrate with existing AuthProvider

### Task 3: Token Management
- [ ] Implement XTokenManager interface
- [ ] Secure token storage using platform keychain
- [ ] Token expiry detection and refresh logic
- [ ] Thread-safe token operations
- [ ] Token cleanup and revocation

### Task 4: Configuration Management
- [ ] Implement XOAuth2Configuration interface
- [ ] Runtime provider configuration
- [ ] Configuration validation
- [ ] Default provider configurations
- [ ] Import/export functionality

### Task 5: Enhanced AuthProvider
- [ ] Extend existing AuthProvider class
- [ ] Maintain backward compatibility
- [ ] Integrate with new OAuth2 service
- [ ] Enhanced error handling
- [ ] Comprehensive logging

## Testing Strategy for Phase 1

### Unit Tests
```cpp
// ucb/qa/ucp/oauth2/test_oauth2_service.cxx
class OAuth2ServiceTest : public CppUnit::TestFixture
{
public:
    void testTokenValidation();
    void testProviderConfiguration();
    void testMultiAccountSupport();
    void testPKCEGeneration();
    void testTokenExpiry();
};
```

### Integration Tests
- OAuth2 service registration and discovery
- Configuration persistence and retrieval
- Token storage and retrieval
- AuthProvider compatibility

### Manual Testing
- Service can be instantiated
- Configuration can be set and retrieved
- Token storage works with platform keychain
- Existing CMIS provider still works

## Success Criteria for Phase 1

✅ **Functional**
- All new UNO services can be instantiated
- Configuration can be set and persisted
- Token storage works securely
- Existing Google Drive integration unchanged

✅ **Technical**
- Code compiles without warnings
- All unit tests pass
- No memory leaks detected
- Thread-safe operations verified

✅ **Compatibility**
- Existing CMIS provider unchanged
- No breaking changes to UCB interfaces
- Google Drive still works as before
- Build system cleanly integrates new module

## Risk Mitigation

**Risk**: UNO service registration failures
**Mitigation**: Comprehensive component testing, step-by-step service enablement

**Risk**: Configuration corruption
**Mitigation**: Configuration validation, backup/restore functionality

**Risk**: Token storage security
**Mitigation**: Platform keychain integration, encryption validation

**Risk**: Threading issues
**Mitigation**: Mutex protection, thread-safe design patterns

**Risk**: Build system conflicts
**Mitigation**: Minimal build changes, isolated module design

## Phase 1 Deliverables

1. ✅ New OAuth2 UNO service module
2. ✅ Runtime configuration system
3. ✅ Enhanced token management
4. ✅ Backward-compatible AuthProvider
5. ✅ Comprehensive test suite
6. ✅ Documentation and build instructions
