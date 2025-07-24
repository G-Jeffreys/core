#!/bin/bash

# Debug script to help locate Google Drive integration in LibreOffice UI
echo "🔍 Google Drive Integration Debug Helper"
echo "========================================"

echo "✅ Checking OAuth Configuration..."
if grep -q "652946642777" config_host/config_oauth2.h; then
    echo "   ✅ OAuth Client ID configured correctly"
else
    echo "   ❌ OAuth Client ID missing!"
    exit 1
fi

if grep -q "GOCSPX-" config_host/config_oauth2.h; then
    echo "   ✅ OAuth Client Secret configured correctly"
else
    echo "   ❌ OAuth Client Secret missing!"
    exit 1
fi

echo ""
echo "📍 WHERE TO FIND GOOGLE DRIVE INTEGRATION:"
echo "=========================================="
echo ""
echo "🎯 PRIMARY LOCATIONS TO CHECK:"
echo "   1. File → Open Remote Files"
echo "   2. File → Open → [Remote Files button]"
echo "   3. Start Center → Open Remote Files"
echo "   4. File → Open → Add Network Location/Service"
echo ""
echo "🔍 WHAT TO LOOK FOR:"
echo "   • 'Google Drive' in server type dropdown"
echo "   • 'Add Service' or 'Add Place' dialog"
echo "   • Remote file picker with service list"
echo "   • Network locations sidebar"
echo ""
echo "⚠️  TROUBLESHOOTING TIPS:"
echo "   • Make sure LibreOffice completely restarted after OAuth config"
echo "   • Look for 'Remote' or 'Network' options in File menu"
echo "   • Check if 'Google Drive' appears in service dropdown lists"
echo "   • Try creating a new document first, then File → Save As → Remote"
echo ""
echo "📊 EXPECTED BEHAVIOR:"
echo "   • Google Drive should be listed FIRST (before OneDrive/Alfresco)"
echo "   • Should show 'Google Drive' (not generic 'CMIS')"
echo "   • Should trigger OAuth flow when selected"
echo ""
echo "🚀 LibreOffice should now be running..."
echo "    Follow the steps above to locate Google Drive integration!"
echo ""
echo "📝 Document your findings in: exploratory_docs/test_results_20250723.md"
