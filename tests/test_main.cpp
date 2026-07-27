#include "Cleaner.hpp"
#include "ContentInspector.hpp"
#include "ProcessManager.hpp"
#include "Logger.hpp"

#include <iostream>
#include <cassert>
#include <string>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void TestHexHashDetector() {
    std::cout << "[TEST] Running Hex Hash Detector Tests... ";
    assert(ContentInspector::IsHexHashFilename("d9aca0098b1b7dcabe416854374248eb628146a0") == true);
    assert(ContentInspector::IsHexHashFilename("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == true);
    assert(ContentInspector::IsHexHashFilename("not_a_hash_file.txt") == false);
    assert(ContentInspector::IsHexHashFilename("12345") == false);
    std::cout << "\033[32mPASSED\033[0m\n";
}

void TestSizeFormatter() {
    std::cout << "[TEST] Running Size Formatter Tests... ";
    assert(Cleaner::FormatSize(500) == "500.00 B");
    assert(Cleaner::FormatSize(1024) == "1.00 KB");
    assert(Cleaner::FormatSize(1048576) == "1.00 MB");
    assert(Cleaner::FormatSize(1073741824) == "1.00 GB");
    std::cout << "\033[32mPASSED\033[0m\n";
}

void TestDurationParser() {
    std::cout << "[TEST] Running Duration Parser Tests... ";
    assert(ContentInspector::ParseDurationToMinutes("30m") == 30);
    assert(ContentInspector::ParseDurationToMinutes("2h") == 120);
    assert(ContentInspector::ParseDurationToMinutes("1d") == 1440);
    assert(ContentInspector::ParseDurationToMinutes("1w") == 10080);
    std::cout << "\033[32mPASSED\033[0m\n";
}

void TestSizeParser() {
    std::cout << "[TEST] Running Size Parser Tests... ";
    assert(ContentInspector::ParseSizeToBytes("1024b") == 1024);
    assert(ContentInspector::ParseSizeToBytes("10kb") == 10240);
    assert(ContentInspector::ParseSizeToBytes("50mb") == 52428800);
    assert(ContentInspector::ParseSizeToBytes("1gb") == 1073741824);
    std::cout << "\033[32mPASSED\033[0m\n";
}

void TestContentProtectionShield() {
    std::cout << "[TEST] Running Content Protection Shield Tests... ";
    InspectionConfig cfg;
    
    // Create temporary test files
    fs::path tempDir = fs::temp_directory_path() / "sys_cleaner_unit_test";
    fs::create_directories(tempDir);

    fs::path userCodeFile = tempDir / "script.py";
    {
        std::ofstream f(userCodeFile);
        f << "print('Hello World')\n";
    }

    fs::path junkLogFile = tempDir / "debug.log";
    {
        std::ofstream f(junkLogFile);
        f << "DEBUG TRACE LOG\n";
    }

    // Assert python source code is PROTECTED even in temp dir
    assert(ContentInspector::InspectFile(userCodeFile, cfg) == FileJunkType::ProtectedUserFile);
    // Assert .log file is recognized as DEFINITE JUNK
    assert(ContentInspector::InspectFile(junkLogFile, cfg) == FileJunkType::DefiniteJunk);

    fs::remove_all(tempDir);
    std::cout << "\033[32mPASSED\033[0m\n";
}

void TestProcessManager() {
    std::cout << "[TEST] Running Process Manager Diagnostic Tests... ";
    size_t count = ProcessManager::StopLockingProcesses(false); // Diagnostic scan without killing
    std::cout << "\033[32mPASSED\033[0m (Found " << count << " lock handles)\n";
}

int main() {
    std::cout << "\033[1;36m====================================================================\n"
              << "   Gemini System Cleaner v3.5 - Unit Test & TDD Regression Suite    \n"
              << "====================================================================\033[0m\n\n";

    TestHexHashDetector();
    TestSizeFormatter();
    TestDurationParser();
    TestSizeParser();
    TestContentProtectionShield();
    TestProcessManager();

    std::cout << "\n\033[1;32mALL UNIT TESTS PASSED SUCCESSFULLY! (100% REGRESSION TEST PASS)\033[0m\n";
    return 0;
}
