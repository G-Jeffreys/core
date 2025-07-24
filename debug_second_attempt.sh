#!/bin/bash

# LibreOffice Google Drive Second Attempt Freeze Diagnostic
echo "🔬 LibreOffice Second Attempt Freeze Diagnostic"
echo "================================================"

# Kill any existing LibreOffice processes
echo "1. Killing any existing LibreOffice processes..."
killall soffice 2>/dev/null || echo "   No LibreOffice processes found"

# Start LibreOffice with comprehensive debugging
echo "2. Starting LibreOffice with enhanced debugging..."
export SAL_LOG="+WARN+INFO.writerperfect+INFO.filter.config"

echo "3. Instructions for manual testing:"
echo ""
echo "   📋 EXACT TEST SEQUENCE:"
echo "   ======================"
echo "   1. File → Open → Remote Files"
echo "   2. Select your test Google Drive file"
echo "   3. Click Open"
echo "   4. When dialog appears asking 'Do you want to repair this document?'"
echo "   5. Click 'YES' (this triggers repair mode)"
echo "   6. Watch for freeze vs normal error handling"
echo ""
echo "   🔍 EXPECTED DEBUG OUTPUT:"
echo "   ========================"
echo "   First attempt:"
echo "   - WPSDocument::isFileFormatSupported()"
echo "   - Throwing InvalidHeaderException"
echo "   - Dialog appears"
echo ""
echo "   Second attempt (after clicking YES):"
echo "   - Should show error dialog, NOT freeze"
echo ""
echo "   💡 WHAT TO WATCH FOR:"
echo "   ====================="
echo "   ✅ Good: Error dialog appears"
echo "   ❌ Bad: LibreOffice becomes unresponsive/frozen"
echo ""

# Start LibreOffice
./instdir/LibreOfficeDev.app/Contents/MacOS/soffice --writer &

echo "4. LibreOffice started. Please follow the test sequence above."
echo "5. If freeze occurs, check Activity Monitor for LibreOffice CPU usage"
echo "6. Report your findings!"
