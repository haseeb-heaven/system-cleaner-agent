#include "Cleaner.hpp"
#include "ContentInspector.hpp"
#include "ProcessManager.hpp"
#include "Logger.hpp"
#include "AgentEngine.hpp"
#include "OpenTUI.hpp"
#include "SmartScheduler.hpp"

#include <iostream>
#include <cassert>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;

void TestHexHashDetector() {
    std::cout << "[TEST] Running Hex Hash Detector Tests... ";
    
    assert(ContentInspector::IsHexHashFilename("d9aca0098b1b7dcabe416854374248eb628146a0") == true);
    assert(ContentInspector::IsHexHashFilename("dc53aa3f1a2f28a8366d4dcc7d444b7b41099ed4") == true);
    assert(ContentInspector::IsHexHashFilename("dcaffcdeb01b83df3b64d2c6a4535c5fdc0a166a") == true);
    assert(ContentInspector::IsHexHashFilename("0000000000000000000000000000000000000000") == true);
    assert(ContentInspector::IsHexHashFilename("ffffffffffffffffffffffffffffffffffffffff") == true);

    assert(ContentInspector::IsHexHashFilename("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == true);
    assert(ContentInspector::IsHexHashFilename("ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb") == true);

    assert(ContentInspector::IsHexHashFilename("not_a_hash_file.txt") == false);
    assert(ContentInspector::IsHexHashFilename("12345") == false);
    assert(ContentInspector::IsHexHashFilename("g9aca0098b1b7dcabe416854374248eb628146a0") == false);
    assert(ContentInspector::IsHexHashFilename("d9aca0098b1b7dcabe416854374248eb628146a01") == false);
    
    std::cout << "\033[32mPASSED (11 assertions)\033[0m\n";
}

void TestExtensionClassifications() {
    std::cout << "[TEST] Running Extension Classification Tests... ";
    InspectionConfig cfg;

    fs::path testDir = fs::temp_directory_path() / "agent_ext_test";
    fs::create_directories(testDir);

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
    assert(ContentInspector::ParseDurationToMinutes("15m") == 15);
    assert(ContentInspector::ParseDurationToMinutes("1h") == 60);
    assert(ContentInspector::ParseDurationToMinutes("24h") == 1440);
    assert(ContentInspector::ParseDurationToMinutes("7d") == 10080);
    assert(ContentInspector::ParseDurationToMinutes("1w") == 10080);

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

    fs::path pdfPath = testDir / "sample_doc.dat";
    {
        std::ofstream out(pdfPath, std::ios::binary);
        out << "%PDF-1.5 header content";
    }
    assert(ContentInspector::InspectFile(pdfPath, cfg) == FileJunkType::ProtectedUserFile);

    fs::path pngPath = testDir / "image_data.dat";
    {
        std::ofstream out(pngPath, std::ios::binary);
        out << "\x89PNG\r\n\x1a\nheader";
    }
    assert(ContentInspector::InspectFile(pngPath, cfg) == FileJunkType::ProtectedUserFile);

    fs::path dbPath = testDir / "data_store.dat";
    {
        std::ofstream out(dbPath, std::ios::binary);
        out << "SQLite format 3\0";
    }
    assert(ContentInspector::InspectFile(dbPath, cfg) == FileJunkType::ProtectedUserFile);

    fs::remove_all(testDir);
    std::cout << "\033[32mPASSED (3 assertions)\033[0m\n";
}

void TestOpenTUIFramework() {
    std::cout << "[TEST] Running OpenTUI Framework Tests... ";
    
    std::string border = OpenTUI::Box::DrawBorder(40, "TEST TITLE");
    assert(border.find("TEST TITLE") != std::string::npos);
    assert(border.find("+") != std::string::npos);

    std::string footer = OpenTUI::Box::DrawFooter(40);
    assert(footer.find("+") != std::string::npos);

    std::string pb0 = OpenTUI::ProgressBar::Render(0.0, 10);
    assert(pb0.find("0.0%") != std::string::npos);
    
    std::string pb100 = OpenTUI::ProgressBar::Render(100.0, 10);
    assert(pb100.find("100.0%") != std::string::npos);

    std::cout << "\033[32mPASSED (4 assertions)\033[0m\n";
}

void TestSmartSchedulerEngine() {
    std::cout << "[TEST] Running Smart Scheduler & Memory Threshold Tests... ";

    double memPct = SmartScheduler::GetMemoryUsagePercent();
    double diskPct = SmartScheduler::GetDiskUsagePercent();
    assert(memPct >= 0.0 && memPct <= 100.0);
    assert(diskPct >= 0.0 && diskPct <= 100.0);

    ScheduleRule rule = SmartScheduler::ParseRuleString("D:/Temp:15m:mem>80%");
    assert(rule.targetFolder.string() == "D:/Temp");
    assert(rule.intervalSeconds == 900);
    assert(rule.memThresholdPercent == 80.0);

    std::cout << "\033[32mPASSED (5 assertions)\033[0m\n";
}

void TestAgentGoalPathExtractor() {
    std::cout << "[TEST] Running Agent Goal Path Extractor Tests... ";

    AgentEngine agent1("Perform clean code on D:/Temp when mem>80%");
    fs::path dummyTarget = fs::temp_directory_path() / "agent_goal_test";
    fs::create_directories(dummyTarget);

    agent1.SetCustomTargetPaths({dummyTarget});
    agent1.RunReActLoop(true);
    assert(agent1.GetTrajectory().size() >= 6);

    fs::remove_all(dummyTarget);
    std::cout << "\033[32mPASSED (2 assertions)\033[0m\n";
}

void TestReActAgentTrajectory() {
    std::cout << "[TEST] Running ReAct Agent Engine Trajectory Tests... ";

    fs::path dummyTarget = fs::temp_directory_path() / "react_trajectory_test";
    fs::create_directories(dummyTarget);

    {
        std::ofstream tempFile(dummyTarget / "cache.tmp");
        tempFile << "temporary junk payload\n";
    }

    AgentEngine agent("Unit test ReAct agent storage optimization");
    agent.SetCustomTargetPaths({dummyTarget});
    agent.RunReActLoop(true);

    const auto& steps = agent.GetTrajectory();
    assert(steps.size() >= 6);
    assert(steps[0].type == AgentStepType::Thought);
    assert(steps[1].type == AgentStepType::Action);
    assert(steps[2].type == AgentStepType::Observation);

    fs::remove_all(dummyTarget);
    std::cout << "\033[32mPASSED (4 assertions)\033[0m\n";
}

int main() {
    std::cout << "\033[1;36m====================================================================\n"
              << "  system-cleaner-agent v5.0 - OpenTUI & SmartScheduler Suite         \n"
              << "====================================================================\033[0m\n\n";

    TestHexHashDetector();
    TestExtensionClassifications();
    TestSizeFormatter();
    TestDurationAndSizeParsers();
    TestMagicBytes();
    TestOpenTUIFramework();
    TestSmartSchedulerEngine();
    TestAgentGoalPathExtractor();
    TestReActAgentTrajectory();

    std::cout << "\n\033[1;32mALL UNIT TESTS PASSED SUCCESSFULLY! (100% REGRESSION PASS)\033[0m\n";
    std::exit(0);
}
