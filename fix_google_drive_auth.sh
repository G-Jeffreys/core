#!/bin/bash

# LibreOffice Google Drive Authentication Fix
echo "🔧 LibreOffice Google Drive Authentication Fix"
echo "=============================================="
echo ""

echo "🎯 FIXING THE MOST COMMON ISSUE: EXPIRED OAUTH TOKENS"
echo "======================================================"
echo ""

# Kill any running LibreOffice processes
echo "1. Killing any running LibreOffice processes..."
killall soffice 2>/dev/null || echo "   No LibreOffice processes found"
echo ""

# Clear OAuth cache
echo "2. Clearing cached OAuth authentication data..."
AUTH_PATH="$HOME/.config/libreoffice-dev/4/user/registrymodifications.xcu"
if [ -f "$AUTH_PATH" ]; then
    echo "   Found auth cache: $AUTH_PATH"
    rm -f "$AUTH_PATH"
    echo "   ✅ Cleared OAuth cache"
else
    echo "   No OAuth cache found (that's fine)"
fi
echo ""

# Clear any other potential auth caches
echo "3. Clearing additional authentication caches..."
ADDITIONAL_PATHS=(
    "$HOME/.config/libreoffice-dev/4/user/autotextuser/"
    "$HOME/.config/libreoffice-dev/4/user/config/webdav_servers.xml"
    "$HOME/.config/libreoffice-dev/4/user/psprint/"
)

for path in "${ADDITIONAL_PATHS[@]}"; do
    if [ -e "$path" ]; then
        echo "   Clearing: $path"
        rm -rf "$path"
    fi
done
echo "   ✅ Additional caches cleared"
echo ""

# Verify OAuth credentials are configured
echo "4. Verifying OAuth credentials are configured..."
if grep -q "GDRIVE_CLIENT_ID" config_host/config_oauth2.h 2>/dev/null; then
    CLIENT_ID=$(grep "GDRIVE_CLIENT_ID" config_host/config_oauth2.h | cut -d'"' -f2)
    if [ -n "$CLIENT_ID" ] && [ "$CLIENT_ID" != "" ]; then
        echo "   ✅ OAuth Client ID configured: ${CLIENT_ID:0:20}..."
    else
        echo "   ❌ OAuth Client ID is empty!"
        exit 1
    fi
else
    echo "   ❌ OAuth configuration not found!"
    echo "   Make sure LibreOffice was built with Google Drive credentials."
    exit 1
fi
echo ""

# Start fresh LibreOffice
echo "5. Starting LibreOffice with fresh authentication state..."
export SAL_LOG="+WARN+INFO.ucb.ucp.cmis"
./instdir/LibreOfficeDev.app/Contents/MacOS/soffice --writer &
echo "   ✅ LibreOffice started"
echo ""

echo "🎯 NEXT STEPS FOR RE-AUTHENTICATION:"
echo "==================================="
echo ""
echo "1. In LibreOffice, go to: File → Open Remote Files"
echo "2. Select 'Google Drive' from the service list"
echo "3. You should see an OAuth dialog (NOT username/password)"
echo "4. Follow the OAuth flow:"
echo "   a) Browser opens to Google consent screen"
echo "   b) Sign in to your Google account"
echo "   c) Grant LibreOffice access to Drive"
echo "   d) Copy authorization code back to LibreOffice"
echo "5. You should now see your Google Drive files"
echo ""

echo "💡 WHAT THIS FIXES:"
echo "=================="
echo ""
echo "✅ Expired OAuth tokens → Fresh authentication"
echo "✅ Cached auth errors → Clean slate"
echo "✅ Permission issues → New consent flow"
echo "✅ Download corruption → Valid credentials for file access"
echo ""

echo "🚨 IF THIS DOESN'T WORK:"
echo "========================"
echo ""
echo "The issue may be:"
echo "• Google account security settings"
echo "• Network/firewall blocking OAuth"
echo "• Google API quota/rate limiting"
echo "• 2FA interfering with OAuth flow"
echo ""
echo "Run ./diagnose_oauth_issues.sh for detailed debugging"
echo ""
echo "✅ Authentication reset complete!"
