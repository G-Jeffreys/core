#!/bin/bash

# Clean Google Drive Diagnostic
clear
echo "🔍 GOOGLE DRIVE DIAGNOSTIC - Clean Output"
echo "========================================="
echo ""

# Kill LibreOffice quietly
killall soffice 2>/dev/null

echo "🚀 Starting LibreOffice with Google Drive debugging..."

# Create a log filter script
cat > /tmp/filter_log.sh << 'EOF'
#!/bin/bash
while IFS= read -r line; do
    # Only show important Google Drive related messages
    if echo "$line" | grep -E "(WPSDocument|OAuth|CMIS|Google|Drive|transferFailed|InvalidHeaderException|Throwing|libcmis)" > /dev/null; then
        echo "🔍 $line"
    fi
done
EOF
chmod +x /tmp/filter_log.sh

# Start LibreOffice with filtered output
export SAL_LOG="+ERROR.writerperfect+INFO.ucb.ucp.cmis"
./instdir/LibreOfficeDev.app/Contents/MacOS/soffice --writer 2>&1 | /tmp/filter_log.sh &

sleep 2
echo "✅ LibreOffice started with Google Drive monitoring"
echo ""
echo "📋 MONITORED TEST:"
echo "=================="
echo ""
echo "1. File → Open Remote Files → Google Drive"
echo "2. Try to open ANY file"
echo "3. Watch for error messages above"
echo ""
echo "🔍 LOOKING FOR:"
echo "• OAuth/authentication errors"
echo "• Download/transfer errors"
echo "• Format detection crashes"
echo "• Any 'WPSDocument' or 'InvalidHeaderException' messages"
echo ""
echo "⏱️  Let it run for 30 seconds even if it seems frozen"
echo "📝 Copy any error messages you see above"
echo ""
echo "When done, press Ctrl+C to stop monitoring"

# Keep the script running to maintain the filter
wait
