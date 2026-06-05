//-------------------------------------------------------------------------------------
// LogParser.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LogParser.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace LogManager {

namespace {

// String utility functions
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string trim(const std::string& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    if (begin == value.end()) {
        return std::string();
    }

    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();

    return std::string(begin, end);
}

std::string replaceAll(std::string value, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return value;
    }

    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.length(), to);
        position += to.length();
    }
    return value;
}

std::string normalizeNewlines(std::string value) {
    value = replaceAll(std::move(value), "\r\n", "\n");
    value = replaceAll(std::move(value), "\r", "\n");
    return value;
}

std::string simplifyWhitespace(const std::string& value) {
    std::string simplified;
    simplified.reserve(value.size());

    bool previousWasWhitespace = false;
    for (unsigned char character : value) {
        if (std::isspace(character) != 0) {
            if (!previousWasWhitespace) {
                simplified.push_back(' ');
                previousWasWhitespace = true;
            }
            continue;
        }

        simplified.push_back(static_cast<char>(character));
        previousWasWhitespace = false;
    }

    return trim(simplified);
}

std::vector<std::string> splitLines(const std::string& value) {
    std::vector<std::string> lines;
    std::stringstream stream(value);
    std::string line;
    while (std::getline(stream, line, '\n')) {
        lines.push_back(line);
    }
    if (!value.empty() && value.back() == '\n') {
        lines.emplace_back();
    }
    return lines;
}

} // namespace

// PERFORMANCE OPTIMIZATION: Manual parsing instead of regex
// Regex matching is extremely slow for large log files (2+ minutes for thousands of entries)
// Manual parsing reduces parsing time by ~10-20x
bool tryParseLogLine(const std::string& line,
                     std::string* systemTime,
                     std::string* gameTime,
                     std::string* category,
                     std::string* message) {
    // Expected format: [HH:MM:SS][game_time][category]: message
    // Example: [12:34:56][1936.01.01][error]: Something went wrong
    
    if (line.empty() || line[0] != '[') {
        return false;
    }
    
    // Parse system time [HH:MM:SS]
    std::size_t pos = 1;
    std::size_t end = line.find(']', pos);
    if (end == std::string::npos || end - pos < 8) {
        return false;
    }
    *systemTime = line.substr(pos, end - pos);
    
    // Check for second bracket [
    pos = end + 1;
    if (pos >= line.size() || line[pos] != '[') {
        return false;
    }
    
    // Parse game time [...]
    pos++;
    end = line.find(']', pos);
    if (end == std::string::npos) {
        return false;
    }
    *gameTime = line.substr(pos, end - pos);
    
    // Check for third bracket [
    pos = end + 1;
    if (pos >= line.size() || line[pos] != '[') {
        return false;
    }
    
    // Parse category [...]
    pos++;
    end = line.find(']', pos);
    if (end == std::string::npos) {
        return false;
    }
    *category = line.substr(pos, end - pos);
    
    // Check for colon :
    pos = end + 1;
    if (pos >= line.size() || line[pos] != ':') {
        return false;
    }
    
    // Parse message (skip optional space after colon)
    pos++;
    if (pos < line.size() && line[pos] == ' ') {
        pos++;
    }
    
    *message = (pos < line.size()) ? line.substr(pos) : std::string();
    return true;
}

std::vector<LogEntry> LogParser::parseLogContent(const std::string& content) const {
    std::vector<LogEntry> entries;
    entries.reserve(1000); // Pre-allocate for typical log file size
    
    const std::vector<std::string> lines = splitLines(content);

    LogEntry currentEntry;
    bool hasCurrentEntry = false;
    int currentIndex = 0;

    auto finalizeCurrentEntry = [&]() {
        if (!hasCurrentEntry) {
            return;
        }

        currentEntry.message = trim(normalizeNewlines(currentEntry.message));
        currentEntry.isHighPriority =
            toLower(currentEntry.message).find("this will likely crash the game") != std::string::npos;
        currentEntry.normalizedKey = normalizeLogEntryKey(currentEntry.category, currentEntry.message);
        currentEntry.originalIndex = currentIndex++;
        entries.push_back(std::move(currentEntry)); // Use move to avoid copy
        
        currentEntry = LogEntry();
        hasCurrentEntry = false;
    };

    for (const std::string& line : lines) { // Use const reference to avoid copy
        // Remove trailing \r if present
        std::string cleanLine = line;
        if (!cleanLine.empty() && cleanLine.back() == '\r') {
            cleanLine.pop_back();
        }

        std::string systemTime, gameTime, category, message;
        if (tryParseLogLine(cleanLine, &systemTime, &gameTime, &category, &message)) {
            finalizeCurrentEntry();
            currentEntry.systemTime = std::move(systemTime);
            currentEntry.gameTime = std::move(gameTime);
            currentEntry.category = std::move(category);
            currentEntry.message = std::move(message);
            hasCurrentEntry = true;
            continue;
        }

        if (!hasCurrentEntry) {
            continue;
        }

        currentEntry.message += "\n" + cleanLine;
    }

    finalizeCurrentEntry();
    return entries;
}

std::string LogParser::normalizeLogEntryKey(const std::string& category, const std::string& message) const {
    std::string normalized = toLower(trim(category)) + "\n" + normalizeNewlines(message);
    std::vector<std::string> lines = splitLines(normalized);

    std::ostringstream stream;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            stream << '\n';
        }
        stream << simplifyWhitespace(lines[index]);
    }

    return trim(stream.str());
}

} // namespace LogManager
