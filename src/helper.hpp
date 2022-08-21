
// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes) {
    if (bytes < 1024)
        return String(bytes) + " B";
    else if (bytes < (1024 * 1024))
        return String(bytes / 1024.0) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        return String(bytes / 1024.0 / 1024.0) + " MB";
    else
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

// list all of the files, if ishtml=true, return html rather than simple text
String listFiles(bool ishtml = false) {
    String returnText = "";
    Serial.println("Listing files stored on SPIFFS");
    File root = SPIFFS.open("/");
    File foundfile = root.openNextFile();
    if (ishtml) {
        returnText +=
            "<table><tr><th align='left'>Name</th><th "
            "align='left'>Size</th></tr>";
    }
    while (foundfile) {
        if (ishtml) {
            returnText += "<tr align='left'><td>" + String(foundfile.name()) +
                          "</td><td>" + humanReadableSize(foundfile.size()) +
                          "</td></tr>";
        } else {
            returnText += "File: " + String(foundfile.name()) + "\n";
        }
        foundfile = root.openNextFile();
    }
    if (ishtml) {
        returnText += "</table>";
    }
    root.close();
    foundfile.close();
    return returnText;
}