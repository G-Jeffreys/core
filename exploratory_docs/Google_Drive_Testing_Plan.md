# Google Drive Integration Testing Plan

*Testing scenarios for LibreOffice Google Drive connector development*
*Date: 2025-07-23*

## Overview

This document outlines comprehensive testing scenarios for the Google Drive integration in LibreOffice, designed to evaluate current functionality and identify areas for enhancement.

## Pre-Testing Setup

### 1. Environment Verification
- ✅ LibreOffice built with Google Drive OAuth credentials
- ✅ Google Cloud Project with Drive API enabled
- ✅ OAuth 2.0 Client credentials configured
- ✅ Test Google account with Drive access

### 2. Test Data Preparation
Create test files in various formats:
- **Text Documents**: `test_document.odt`, `test_document.docx`
- **Spreadsheets**: `test_spreadsheet.ods`, `test_spreadsheet.xlsx`
- **Presentations**: `test_presentation.odp`, `test_presentation.pptx`
- **Various Sizes**:
  - Small (< 1 MB): Basic text document
  - Medium (1-10 MB): Document with images
  - Large (> 10 MB): Presentation with media

## Testing Scenarios

### Scenario 1: Initial Connection & Authentication

**Objective**: Test OAuth2 flow and initial Google Drive connection

**Steps**:
1. **Launch LibreOffice**: `open instdir/LibreOfficeDev.app`
2. **Access Google Drive**: File → Open → Remote Files → Google Drive
3. **OAuth Authentication Flow**: Complete OAuth2 authentication
4. **Success Criteria**:
   - ✅ OAuth dialog appears
   - ✅ Browser opens to Google consent screen
   - ✅ LibreOffice shows Google Drive contents
   - ✅ Authentication persists between sessions

### Scenario 2: File Operations

**Test Cases**:
- **Opening Files**: Various formats (ODT, DOCX, ODS, XLSX, etc.)
- **Saving Files**: Save existing, Save As, Save new documents
- **Browsing**: Navigate folders, search files

**Success Criteria**:
- ✅ Files open correctly and quickly
- ✅ Saving works without data loss
- ✅ Folder navigation is smooth

### Scenario 3: Performance & Error Handling

**Performance Metrics**:
- File open time (target: < 5 seconds for 1MB file)
- Save time (target: < 5 seconds for 1MB file)
- Authentication time

**Error Conditions**:
- Network disconnection
- Invalid credentials
- File conflicts
- Large file handling

## Test Execution Scripts

### Quick Test Script
```bash
#!/bin/bash
echo "=== LibreOffice Google Drive Test ==="
echo "1. Starting LibreOffice..."
open instdir/LibreOfficeDev.app
echo "2. Manual verification checklist:"
echo "   □ File → Open → Remote Files shows Google Drive"
echo "   □ OAuth authentication works"
echo "   □ Can browse Google Drive files"
echo "   □ Can open and edit documents"
echo "   □ Can save changes back to Drive"
```

## Expected Findings

Based on our CMIS analysis, we expect:
- **Working but basic** Google Drive integration
- **Manual OAuth code entry** (enhancement opportunity)
- **Generic CMIS interface** (could be Drive-branded)
- **Limited error handling** (room for improvement)

## Enhancement Opportunities

1. **Improve OAuth UX**: Automatic browser flow vs. manual code entry
2. **Add Progress Indicators**: Upload/download progress bars
3. **Drive-Specific UI**: Google branding and Drive-specific features
4. **Better Error Handling**: Clear messages and recovery options
5. **Performance Optimization**: Faster file operations
6. **Conflict Resolution**: Better handling of file version conflicts
