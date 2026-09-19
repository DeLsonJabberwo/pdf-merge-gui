WriteRegStr HKLM "Software\Classes\PDFMerge.Document" "" "PDF Merge Document"
WriteRegStr HKLM "Software\Classes\PDFMerge.Document\DefaultIcon" "" "$INSTDIR\bin\pdf-merge.exe,0"
WriteRegStr HKLM "Software\Classes\PDFMerge.Document\shell\open\command" "" '"$INSTDIR\bin\pdf-merge.exe" "%1"'
WriteRegStr HKLM "Software\PDFMerge\Capabilities" "ApplicationName" "PDF Merge"
WriteRegStr HKLM "Software\PDFMerge\Capabilities" "ApplicationDescription" "Arrange PDF pages and export one document"
WriteRegStr HKLM "Software\PDFMerge\Capabilities\FileAssociations" ".pdf" "PDFMerge.Document"
WriteRegStr HKLM "Software\RegisteredApplications" "PDF Merge" "Software\PDFMerge\Capabilities"
