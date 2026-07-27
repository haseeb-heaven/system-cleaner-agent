#include "Cleaner.hpp"
#include "ContentInspector.hpp"
#include "ProcessManager.hpp"
#include "Logger.hpp"
#include "AgentEngine.hpp"

#include <iostream>
#include <cassert>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

void TestHexHashDetector() {
    std::cout << "[TEST] Running Hex Hash Detector Tests... ";
    
    // 40-character SHA-1 Hashes
    assert(ContentInspector::IsHexHashFilename("d9aca0098b1b7dcabe416854374248eb628146a0") == true);
    assert(ContentInspector::IsHexHashFilename("dc53aa3f1a2f28a8366d4dcc7d444b7b41099ed4") == true);
    assert(ContentInspector::IsHexHashFilename("dcaffcdeb01b83df3b64d2c6a4535c5fdc0a166a") == true);
    assert(ContentInspector::IsHexHashFilename("0000000000000000000000000000000000000000") == true);
    assert(ContentInspector::IsHexHashFilename("ffffffffffffffffffffffffffffffffffffffff") == true);

    // 64-character SHA-256 Hashes
    assert(ContentInspector::IsHexHashFilename("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == true);
    assert(ContentInspector::IsHexHashFilename("ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb") == true);

    // Negative Cases
    assert(ContentInspector::IsHexHashFilename("not_a_hash_file.txt") == false);
    assert(ContentInspector::IsHexHashFilename("12345") == false);
    assert(ContentInspector::IsHexHashFilename("g9aca0098b1b7dcabe416854374248eb628146a0") == false); // 'g' is invalid hex
    assert(ContentInspector::IsHexHashFilename("d9aca0098b1b7dcabe416854374248eb628146a01") == false); // 41 chars
    
    std::cout << "\033[32mPASSED (11 assertions)\033[0m\n";
}

void TestExtensionClassifications() {
    std::cout << "[TEST] Running Extension Classification Tests... ";
    InspectionConfig cfg;

    fs::path testDir = fs::temp_directory_path() / "agent_ext_test";
    fs::create_directories(testDir);

    // Source & Document Protected Extensions
    std::vector<std::string> protectedExts = {
        "f.py", "f.cpp", "f.c", "f.h", "f.hpp", "f.cs", "f.java", "f.js", "f.ts", "f.tsx",
        "f.html", "f.css", "f.json", "f.sql", "f.sh", "f.bat", "f.cmd", "f.ps1", "f.md",
        "f.pdf", "f.doc", "f.docx", "f.xls", "f.xlsx", "f.ppt", "f.pptx", "f.zip", "f.rar",
        "f.png", "f.jpg", "f.jpeg", "f.svg", "f.mp4", "f.db", "f.sqlite"
    };

    for (const auto& fname : protectedExts) {
        fs::path p = testDir / fname;
        { std::ofstream out(p); out << "dummy content\n"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::ProtectedUserFile);
    }

    // Junk Extensions
    std::vector<std::string> junkExts = {
        "f.tmp", "f.temp", "f.bak", "f.old", "f.log", "f.dmp", "f.mdmp", "f.wer", "f.chk",
        "f.crash", "f.swp", "f.swo", "f.part", "f.pyc", "f.pyo", "f.obj", "f.o", "f.pdb"
    };

    for (const auto& fname : junkExts) {
        fs::path p = testDir / fname;
        { std::ofstream out(p); out << "junk content\n"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::DefiniteJunk);
    }

    fs::remove_all(testDir);
    std::cout << "\033[32mPASSED (" << (protectedExts.size() + junkExts.size()) << " assertions)\033[0m\n";
}

void TestSizeFormatter() {
    std::cout << "[TEST] Running Size Formatter Tests... ";
    assert(Cleaner::FormatSize(0) == "0 B");
    assert(Cleaner::FormatSize(500) == "500.00 B");
    assert(Cleaner::FormatSize(1024) == "1.00 KB");
    assert(Cleaner::FormatSize(1536) == "1.50 KB");
    assert(Cleaner::FormatSize(1048576) == "1.00 MB");
    assert(Cleaner::FormatSize(15728640) == "15.00 MB");
    assert(Cleaner::FormatSize(1073741824) == "1.00 GB");
    assert(Cleaner::FormatSize(5368709120ULL) == "5.00 GB");
    std::cout << "\033[32mPASSED (8 assertions)\033[0m\n";
}

void TestDurationAndSizeParsers() {
    std::cout << "[TEST] Running Duration & Size Parser Tests... ";
    // Duration Parsing
    assert(ContentInspector::ParseDurationToMinutes("15m") == 15);
    assert(ContentInspector::ParseDurationToMinutes("1h") == 60);
    assert(ContentInspector::ParseDurationToMinutes("24h") == 1440);
    assert(ContentInspector::ParseDurationToMinutes("7d") == 10080);
    assert(ContentInspector::ParseDurationToMinutes("1w") == 10080);

    // Size Parsing
    assert(ContentInspector::ParseSizeToBytes("512b") == 512);
    assert(ContentInspector::ParseSizeToBytes("10kb") == 10240);
    assert(ContentInspector::ParseSizeToBytes("100mb") == 104857600);
    assert(ContentInspector::ParseSizeToBytes("2gb") == 2147483648ULL);
    std::cout << "\033[32mPASSED (9 assertions)\033[0m\n";
}

void TestMagicBytes() {
    std::cout << "[TEST] Running Magic Bytes Inspection Tests... ";
    fs::path testDir = fs::temp_directory_path() / "magic_bytes_test";
    fs::create_directories(testDir);

    InspectionConfig cfg;

    // PDF Magic Bytes: %PDF-1.5
    fs::path pdfPath = testDir / "sample_doc.dat";
    {
        std::ofstream out(pdfPath, std::ios::binary);
        out << "%PDF-1.5 header content";
    }
    assert(ContentInspector::InspectFile(pdfPath, cfg) == FileJunkType::ProtectedUserFile);

    // PNG Magic Bytes: \x89PNG
    fs::path pngPath = testDir / "image_data.dat";
    {
        std::ofstream out(pngPath, std::ios::binary);
        out << "\x89PNG\r\n\x1a\nheader";
    }
    assert(ContentInspector::InspectFile(pngPath, cfg) == FileJunkType::ProtectedUserFile);

    // SQLite Magic Bytes: SQLite format 3
    fs::path dbPath = testDir / "data_store.dat";
    {
        std::ofstream out(dbPath, std::ios::binary);
        out << "SQLite format 3\0";
    }
    assert(ContentInspector::InspectFile(dbPath, cfg) == FileJunkType::ProtectedUserFile);

    fs::remove_all(testDir);
    std::cout << "\033[32mPASSED (3 assertions)\033[0m\n";
}

void TestReActAgentTrajectory() {
    std::cout << "[TEST] Running ReAct Agent Engine Trajectory Tests... ";
    AgentEngine agent("Unit test ReAct agent storage optimization");
    agent.RunReActLoop(true); // Run dry-run loop

    const auto& steps = agent.GetTrajectory();
    assert(steps.size() >= 5);
    assert(steps[0].type == AgentStepType::Thought);
    assert(steps[1].type == AgentStepType::Action);
    assert(steps[2].type == AgentStepType::Observation);

    std::cout << "\033[32mPASSED (4 assertions)\033[0m\n";
}

int main() {
    std::cout << "\033[1;36m====================================================================\n"
              << "  SystemCleanerAgent v4.0 - Unit Test & TDD Regression Suite        \n"
              << "====================================================================\033[0m\n\n";

    TestHexHashDetector();
    TestExtensionClassifications();
    TestSizeFormatter();
    TestDurationAndSizeParsers();
    TestMagicBytes();
    TestReActAgentTrajectory();

    std::cout << "\n\033[1;32mALL UNIT TESTS PASSED SUCCESSFULLY! (100% REGRESSION PASS)\033[0m\n";
    return 0;
}
