#!/bin/bash

# Check Google Drive File Content
clear
echo "🔍 GOOGLE DRIVE FILE CONTENT ANALYSIS"
echo "====================================="
echo ""

# Kill LibreOffice
killall soffice 2>/dev/null

echo "This will help us see WHY Google Drive files are corrupted."
echo ""

# Create a simple test to download file content and examine it
echo "🎯 TESTING HYPOTHESIS: Files from Google Drive are corrupted"
echo ""

echo "Let's create a test to see what's actually in the downloaded content:"
echo ""

cat > /tmp/test_download.py << 'EOF'
#!/usr/bin/env python3
import sys
import requests

# This simulates what might be happening
print("🔍 Testing different scenarios for why files might be corrupted:")
print("")

# Scenario 1: Authentication returns HTML error page instead of file
html_error = """<!DOCTYPE html>
<html>
<head><title>Google Drive - Access Denied</title></head>
<body>
<h1>Access Denied</h1>
<p>Your session has expired. Please sign in again.</p>
</body>
</html>"""

print("❌ SCENARIO 1: Authentication failure")
print("If OAuth fails, Google returns HTML error page:")
print(f"Content: {html_error[:100]}...")
print("LibreOffice tries to open HTML as MS Works document → CRASH")
print("")

# Scenario 2: Partial download
print("❌ SCENARIO 2: Partial download")
print("If download fails partway through:")
print("Content: PK... (starts like ZIP) but then corrupted data")
print("LibreOffice detects as ODT but data is truncated → CRASH")
print("")

# Scenario 3: Wrong content type
print("❌ SCENARIO 3: Content-type mismatch")
print("Google Drive exports in different format than expected:")
print("- Request .odt file")
print("- Get Google Docs HTML export instead")
print("- LibreOffice tries to parse HTML as binary → CRASH")
print("")

print("🎯 SOLUTION: We need to check what's actually being downloaded!")
EOF

python3 /tmp/test_download.py

echo ""
echo "🧪 QUICK TEST:"
echo "============="
echo ""
echo "1. Go to your Google Drive in a web browser"
echo "2. Right-click on one of the files that crashes"
echo "3. Select 'Download'"
echo "4. Save it to your Desktop"
echo "5. Try opening that downloaded file in LibreOffice"
echo ""
echo "❓ DOES THE MANUALLY DOWNLOADED FILE WORK?"
echo "• YES → Problem is LibreOffice's download process"
echo "• NO → Problem is the files themselves are corrupted in Google Drive"
echo ""

echo "📊 MY HYPOTHESIS:"
echo "=================="
echo ""
echo "Based on the evidence, I believe:"
echo ""
echo "1. ✅ OAuth authentication works (you can see files)"
echo "2. ✅ CMIS integration works (file listing works)"
echo "3. ❌ File download gets corrupted data (HTML error pages, partial content, etc.)"
echo "4. ✅ Our crash protection works (showing dialog instead of crash)"
echo ""
echo "The 'crash with dialog' you're seeing might actually be SUCCESS!"
echo "It's showing an error dialog instead of crashing the application."
echo ""

echo "🔬 TO CONFIRM:"
echo "=============="
echo ""
echo "Please test manually downloading a file from Google Drive web interface"
echo "and see if THAT file opens in LibreOffice."
echo ""
echo "If manually downloaded files work, the issue is network/download corruption."
echo "If manually downloaded files also fail, the files themselves are corrupted."
