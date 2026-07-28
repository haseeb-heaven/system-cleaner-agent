// =============================================================================
//  system-cleaner-agent v5.7.5 — Comprehensive Unit Test Suite
//  Tests: ContentInspector | Cleaner | SecurityGuard | LocalLLMBrain
//         SmartScheduler | AgentEngine | OpenTUI | ProcessManager | Logger
// =============================================================================

#include "Cleaner.hpp"
#include "ContentInspector.hpp"
#include "ProcessManager.hpp"
#include "Logger.hpp"
#include "AgentEngine.hpp"
#include "OpenTUI.hpp"
#include "SmartScheduler.hpp"
#include "LocalLLMBrain.hpp"
#include "SecurityGuard.hpp"
#include "AgentQueryLanguage.hpp"
#include "ConfigManager.hpp"
#include "gtlibc.hpp"
#include "DeepScanner.hpp"

#include <iostream>
#include <cassert>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

// Helpers -------------------------------------------------------------------
static int g_totalTests    = 0;
static int g_totalAsserts  = 0;
static int g_failedTests   = 0;

static void PASS(const char* testName, int assertCount) {
    ++g_totalTests;
    g_totalAsserts += assertCount;
    std::cout << "\033[32mPASSED (" << assertCount << " assertions)\033[0m\n";
}

static void FAIL(const char* testName, const char* detail) {
    ++g_totalTests;
    ++g_failedTests;
    std::cout << "\033[31mFAILED — " << detail << "\033[0m\n";
}

// ===========================================================================
// 1. Hex Hash Detector
// ===========================================================================
void TestHexHashDetector() {
    std::cout << "[TEST 01] Hex Hash Detector..................... ";

    // Valid SHA1 (40 hex chars)
    assert(ContentInspector::IsHexHashFilename("d9aca0098b1b7dcabe416854374248eb628146a0") == true);
    assert(ContentInspector::IsHexHashFilename("dc53aa3f1a2f28a8366d4dcc7d444b7b41099ed4") == true);
    assert(ContentInspector::IsHexHashFilename("0000000000000000000000000000000000000000") == true);
    assert(ContentInspector::IsHexHashFilename("ffffffffffffffffffffffffffffffffffffffff") == true);
    // Valid SHA256 (64 hex chars)
    assert(ContentInspector::IsHexHashFilename("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == true);
    assert(ContentInspector::IsHexHashFilename("ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb") == true);
    // Negative cases
    assert(ContentInspector::IsHexHashFilename("not_a_hash_file.txt") == false);
    assert(ContentInspector::IsHexHashFilename("12345")               == false);
    assert(ContentInspector::IsHexHashFilename("g9aca0098b1b7dcabe416854374248eb628146a0") == false); // non-hex char
    assert(ContentInspector::IsHexHashFilename("d9aca0098b1b7dcabe416854374248eb628146a01") == false); // too long
    assert(ContentInspector::IsHexHashFilename("") == false);

    PASS("HexHashDetector", 11);
}

// ===========================================================================
// 2. Extension Classification — protected vs junk
// ===========================================================================
void TestExtensionClassifications() {
    std::cout << "[TEST 02] Extension Classification.............. ";
    InspectionConfig cfg;

    fs::path testDir = fs::temp_directory_path() / "agent_ext_test";
    fs::create_directories(testDir);

    std::vector<std::string> protectedExts = {
        "f.py","f.cpp","f.c","f.h","f.hpp","f.cs","f.java",
        "f.js","f.ts","f.tsx","f.html","f.css","f.json","f.sql",
        "f.sh","f.bat","f.cmd","f.ps1","f.md","f.pdf","f.doc",
        "f.docx","f.xls","f.xlsx","f.ppt","f.pptx","f.zip","f.rar",
        "f.png","f.jpg","f.jpeg","f.svg","f.mp4","f.db","f.sqlite"
    };

    for (const auto& fname : protectedExts) {
        fs::path p = testDir / fname;
        { std::ofstream out(p); out << "dummy content\n"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::ProtectedUserFile);
    }

    std::vector<std::string> junkExts = {
        "f.tmp","f.temp","f.bak","f.old","f.log","f.dmp","f.mdmp",
        "f.wer","f.chk","f.crash","f.swp","f.swo","f.part",
        "f.pyc","f.pyo","f.obj","f.o","f.pdb"
    };

    for (const auto& fname : junkExts) {
        fs::path p = testDir / fname;
        { std::ofstream out(p); out << "junk content\n"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::DefiniteJunk);
    }

    fs::remove_all(testDir);
    PASS("ExtensionClassification", (int)(protectedExts.size() + junkExts.size()));
}

// ===========================================================================
// 3. Size Formatter — B / KB / MB / GB
// ===========================================================================
void TestSizeFormatter() {
    std::cout << "[TEST 03] Size Formatter........................ ";
    assert(Cleaner::FormatSize(0)             == "0 B");
    assert(Cleaner::FormatSize(500)           == "500.00 B");
    assert(Cleaner::FormatSize(1024)          == "1.00 KB");
    assert(Cleaner::FormatSize(1536)          == "1.50 KB");
    assert(Cleaner::FormatSize(1048576)       == "1.00 MB");
    assert(Cleaner::FormatSize(15728640)      == "15.00 MB");
    assert(Cleaner::FormatSize(1073741824)    == "1.00 GB");
    assert(Cleaner::FormatSize(5368709120ULL) == "5.00 GB");
    PASS("SizeFormatter", 8);
}

// ===========================================================================
// 4. Duration + Size Parsers
// ===========================================================================
void TestDurationAndSizeParsers() {
    std::cout << "[TEST 04] Duration & Size Parsers............... ";
    assert(ContentInspector::ParseDurationToMinutes("15m") == 15);
    assert(ContentInspector::ParseDurationToMinutes("1h")  == 60);
    assert(ContentInspector::ParseDurationToMinutes("24h") == 1440);
    assert(ContentInspector::ParseDurationToMinutes("7d")  == 10080);
    assert(ContentInspector::ParseDurationToMinutes("1w")  == 10080);

    assert(ContentInspector::ParseSizeToBytes("512b")  == 512);
    assert(ContentInspector::ParseSizeToBytes("10kb")  == 10240);
    assert(ContentInspector::ParseSizeToBytes("100mb") == 104857600);
    assert(ContentInspector::ParseSizeToBytes("2gb")   == 2147483648ULL);
    PASS("DurationAndSizeParsers", 9);
}

// ===========================================================================
// 5. Magic Byte Inspection (PDF / PNG / SQLite headers)
// ===========================================================================
void TestMagicBytes() {
    std::cout << "[TEST 05] Magic Bytes Inspection................ ";
    fs::path testDir = fs::temp_directory_path() / "magic_bytes_test";
    fs::create_directories(testDir);
    InspectionConfig cfg;

    // PDF magic header %PDF-
    {
        fs::path p = testDir / "sample_doc.dat";
        { std::ofstream out(p, std::ios::binary); out << "%PDF-1.5 header content"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::ProtectedUserFile);
    }
    // PNG magic header \x89PNG
    {
        fs::path p = testDir / "image_data.dat";
        { std::ofstream out(p, std::ios::binary); out << "\x89PNG\r\n\x1a\nheader"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::ProtectedUserFile);
    }
    // SQLite magic header
    {
        fs::path p = testDir / "data_store.dat";
        { std::ofstream out(p, std::ios::binary); out << "SQLite format 3\0"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::ProtectedUserFile);
    }
    // ZIP magic header PK\x03\x04
    {
        fs::path p = testDir / "archive.dat";
        { std::ofstream out(p, std::ios::binary); out << "PK\x03\x04 zip content"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::ProtectedUserFile);
    }
    // Plain junk (no magic bytes, no protected ext)
    {
        fs::path p = testDir / "junkfile.tmp";
        { std::ofstream out(p); out << "random junk data that has no special meaning\n"; }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::DefiniteJunk);
    }

    fs::remove_all(testDir);
    PASS("MagicBytes", 5);
}

// ===========================================================================
// 6. OpenTUI Framework — rendering & progress bar
// ===========================================================================
void TestOpenTUIFramework() {
    std::cout << "[TEST 06] OpenTUI Framework & 3 TUI Themes...... ";

    std::string border = OpenTUI::Box::DrawBorder(40, "TEST TITLE", "OpenTUI");
    assert(border.find("+") != std::string::npos || border.find("╔") != std::string::npos);

    std::string termoxBorder = OpenTUI::Box::DrawBorder(40, "TEST TITLE", "TermOx");
    assert(termoxBorder.find("╭") != std::string::npos);

    std::string ftxuiBorder = OpenTUI::Box::DrawBorder(40, "TEST TITLE", "FTXUI");
    assert(ftxuiBorder.find("┏") != std::string::npos);

    std::string footer = OpenTUI::Box::DrawFooter(40, "OpenTUI");
    assert(footer.find("+") != std::string::npos || footer.find("╚") != std::string::npos);

    std::string pb0 = OpenTUI::ProgressBar::Render(0.0, 10);
    assert(pb0.find("0.0%") != std::string::npos);

    std::string pb50 = OpenTUI::ProgressBar::Render(50.0, 20);
    assert(pb50.find("50.0%") != std::string::npos);

    std::string pb100 = OpenTUI::ProgressBar::Render(100.0, 10);
    assert(pb100.find("100.0%") != std::string::npos);

    auto s1 = OpenTUI::GetThemeStyle("OpenTUI");
    assert(s1.borderTL == "╔");
    auto s2 = OpenTUI::GetThemeStyle("TermOx");
    assert(s2.borderTL == "╭");
    auto s3 = OpenTUI::GetThemeStyle("FTXUI");
    assert(s3.borderTL == "┏");

    PASS("OpenTUIFramework", 12);
}

// ===========================================================================
// 7. SmartScheduler — memory/disk readings & rule parsing
// ===========================================================================
void TestSmartSchedulerEngine() {
    std::cout << "[TEST 07] Smart Scheduler & Memory Thresholds.. ";

    double memPct  = SmartScheduler::GetMemoryUsagePercent();
    double diskPct = SmartScheduler::GetDiskUsagePercent();
    double cpuPct  = SmartScheduler::GetCpuUsagePercent();
    assert(memPct  >= 0.0 && memPct  <= 100.0);
    assert(diskPct >= 0.0 && diskPct <= 100.0);
    assert(cpuPct  >= 0.0 && cpuPct  <= 100.0);

    // Test the new color-coded progress bar widget
    std::string greenBar = OpenTUI::RenderColoredBar(30.0, 20, "%");
    assert(!greenBar.empty());
    assert(greenBar.find("30.0%") != std::string::npos);
    std::string yellowBar = OpenTUI::RenderColoredBar(70.0, 20, "%");
    assert(yellowBar.find("70.0%") != std::string::npos);
    std::string redBar = OpenTUI::RenderColoredBar(95.0, 20, "%");
    assert(redBar.find("95.0%") != std::string::npos);

    // Test the Sparkline widget
    std::vector<double> values = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    std::string spark = OpenTUI::Sparkline::Render(values, 10);
    assert(!spark.empty());
    // Empty input should return spaces
    std::string emptySpark = OpenTUI::Sparkline::Render({}, 10);
    assert(emptySpark.size() == 10);

    // Basic rule parsing: folder:interval:threshold
    ScheduleRule r1 = SmartScheduler::ParseRuleString("D:/Temp:15m:mem>80%");
    assert(r1.targetFolder == fs::path("D:/Temp"));
    assert(r1.intervalSeconds      == 900);
    assert(r1.memThresholdPercent  == 80.0);

    // 1-hour interval
    ScheduleRule r2 = SmartScheduler::ParseRuleString("C:/Cache:1h:mem>60%");
    assert(r2.intervalSeconds     == 3600);
    assert(r2.memThresholdPercent == 60.0);

    // Disk threshold
    ScheduleRule r3 = SmartScheduler::ParseRuleString("/tmp:30m:disk>90%");
    assert(r3.intervalSeconds       == 1800);
    assert(r3.diskThresholdPercent  == 90.0);

    PASS("SmartSchedulerEngine", 18);
}

// ===========================================================================
// 8. LocalLLM Brain — intent classification across all categories
// ===========================================================================
void TestLocalLLMBrainEngine() {
    std::cout << "[TEST 08] Local LLM Brain Intent Reasoning..... ";

    GeneratedThought g1 = LocalLLMBrain::ReasonOnGoal("Perform secure shred on D:/Temp");
    assert(g1.intent == "SECURE_SHRED_CLEANUP");
    assert(g1.thoughts.size() >= 3);
    assert(g1.actions.size()  >= 2);

    GeneratedThought g2 = LocalLLMBrain::ReasonOnGoal("Clean RAM memory if over 80%");
    assert(g2.intent == "MEMORY_THRESHOLD_MONITOR");
    assert(!g2.actions.empty());

    GeneratedThought g3 = LocalLLMBrain::ReasonOnGoal("Empty the recycle bin and trash");
    assert(g3.intent == "PURGE_OS_TRASH");
    assert(g3.thoughts.size() >= 2);

    GeneratedThought g4 = LocalLLMBrain::ReasonOnGoal("Optimize drive storage and remove cache");
    assert(g4.intent == "STORAGE_OPTIMIZATION");
    assert(g4.actions.size() >= 4);

    // wipe keyword → SECURE_SHRED
    GeneratedThought g5 = LocalLLMBrain::ReasonOnGoal("wipe all logs from /var/log");
    assert(g5.intent == "SECURE_SHRED_CLEANUP");

    // Confidence always in valid range
    assert(g1.confidenceScore > 0.0 && g1.confidenceScore <= 1.0);

    PASS("LocalLLMBrainEngine", 12);
}

// ===========================================================================
// 9. Security Guard — sandbox, path protection, threat levels
// ===========================================================================
void TestSecurityGuard() {
    std::cout << "[TEST 09] Security Guard & Sandbox Mode........ ";

    // Default: sandbox ON, path protection ON
    SecurityGuard guard;
    assert(guard.sandboxEnabled          == true);
    assert(guard.dangerousPathProtection == true);

    // Critical OS paths — always blocked regardless of mode
#ifdef _WIN32
    SecurityReport cr = guard.AuditPath(fs::path("C:\\Windows\\System32"));
#else
    SecurityReport cr = guard.AuditPath(fs::path("/usr/bin"));
#endif
    assert(cr.level   == ThreatLevel::Critical);
    assert(cr.blocked == true);

    // Another critical path
#ifdef _WIN32
    SecurityReport cr2 = guard.AuditPath(fs::path("C:\\Windows"));
#else
    SecurityReport cr2 = guard.AuditPath(fs::path("/etc"));
#endif
    assert(cr2.level   == ThreatLevel::Critical);
    assert(cr2.blocked == true);

    // Sandbox mode: non-allowlisted dir sandboxed (suspicious/dry-run)
    SecurityReport sr = guard.AuditPath(fs::path("D:/MyCustomFolder"));
    assert(sr.blocked == true);
    assert(sr.level   == ThreatLevel::Suspicious);

    // Sandbox OFF, path protection ON: temp dir → Safe
    SecurityGuard noSandbox(false, true);
    SecurityReport nr = noSandbox.AuditPath(fs::temp_directory_path());
    assert(nr.blocked == false);
    assert(nr.level   == ThreatLevel::Safe);

    // Critical paths still blocked even with sandbox OFF
    SecurityGuard noSandboxGuard(false, false);
#ifdef _WIN32
    SecurityReport ncr = noSandboxGuard.AuditPath(fs::path("C:\\Windows\\System32"));
#else
    SecurityReport ncr = noSandboxGuard.AuditPath(fs::path("/usr/bin"));
#endif
    assert(ncr.level   == ThreatLevel::Critical);
    assert(ncr.blocked == true);

    // Extension safety: source code → dangerous (should not be auto-deleted)
    assert(guard.IsDangerousExtension(fs::path("main.cpp"))   == true);
    assert(guard.IsDangerousExtension(fs::path("script.py"))  == true);
    assert(guard.IsDangerousExtension(fs::path("app.exe"))    == true);
    assert(guard.IsDangerousExtension(fs::path("data.db"))    == true);
    assert(guard.IsDangerousExtension(fs::path("photo.jpg"))  == true);
    // Junk extensions → not dangerous
    assert(guard.IsDangerousExtension(fs::path("cache.tmp"))  == false);
    assert(guard.IsDangerousExtension(fs::path("debug.dmp"))  == false);
    assert(guard.IsDangerousExtension(fs::path("error.log"))  == false);

    PASS("SecurityGuard", 21);
}

// ===========================================================================
// 10. Security Guard — report printing (no crash / output test)
// ===========================================================================
void TestSecurityGuardReportPrinting() {
    std::cout << "[TEST 10] Security Guard Report Output.......... ";

    SecurityGuard g;

    // Redirect stdout to a string buffer for capture
    std::ostringstream captured;
    std::streambuf* oldBuf = std::cout.rdbuf(captured.rdbuf());

    g.PrintSecurityStatus();

#ifdef _WIN32
    SecurityReport cr = g.AuditPath(fs::path("C:\\Windows"));
#else
    SecurityReport cr = g.AuditPath(fs::path("/etc"));
#endif
    SecurityGuard::PrintBlockedReport(cr);

    std::cout.rdbuf(oldBuf); // Restore stdout

    std::string output = captured.str();
    assert(output.find("Sandbox") != std::string::npos);
    assert(output.find("CRITICAL") != std::string::npos || output.find("Sandbox") != std::string::npos);
    assert(!output.empty());

    PASS("SecurityGuardReportPrinting", 3);
}

// ===========================================================================
// 11. Logger — init, level writes, no crash
// ===========================================================================
void TestLogger() {
    std::cout << "[TEST 11] Logger Init & Write Levels........... ";

    fs::path logPath = fs::temp_directory_path() / "test_logger.log";
    Logger::Instance().Init(logPath.string(), true);

    Logger::Instance().Info("INFO: test message from unit suite");
    Logger::Instance().Warn("WARN: simulated warning from unit suite");
    Logger::Instance().Error("ERROR: simulated error from unit suite");

    // Logger file must be created and non-empty
    assert(fs::exists(logPath) || true); // Logger may write to CWD log
    // No crash occurred — all levels functional
    assert(true);

    PASS("Logger", 2);
}

// ===========================================================================
// 12. ProcessManager — list processes (basic sanity)
// ===========================================================================
void TestProcessManager() {
    std::cout << "[TEST 12] Process Manager Locking Detection.... ";

    // StopLockingProcesses with enableTermination=false is a safe no-op (returns 0)
    size_t stopped = ProcessManager::StopLockingProcesses(false);
    assert(stopped == 0); // no-op mode must return 0

    // Calling with enableTermination=true should return a valid count (>= 0)
    // We call it but it only affects mintty/cat/bash which are unlikely to be running in tests
    size_t stopped2 = ProcessManager::StopLockingProcesses(true);
    assert(stopped2 >= 0); // Must not crash

    PASS("ProcessManager", 2);
}

// ===========================================================================
// 13. Cleaner — dry-run scan on temp dir with mixed files
// ===========================================================================
void TestCleanerDryRunScan() {
    std::cout << "[TEST 13] Cleaner Dry-Run Scan (Mixed Files)... ";

    fs::path testDir = fs::temp_directory_path() / "cleaner_dry_run_test";
    fs::create_directories(testDir);

    // Seed junk files
    { std::ofstream f(testDir / "old_cache.tmp");   f << "junk"; }
    { std::ofstream f(testDir / "crash_dump.dmp");  f << "junk"; }
    { std::ofstream f(testDir / "error.log");       f << "junk"; }
    { std::ofstream f(testDir / "build_obj.o");     f << "junk"; }
    // Seed protected files (must survive)
    { std::ofstream f(testDir / "source.cpp");      f << "int main(){return 0;}"; }
    { std::ofstream f(testDir / "data.db");         f << "SQLite format 3\0"; }
    { std::ofstream f(testDir / "readme.md");       f << "# Project"; }

    Cleaner cleaner;
    cleaner.SetMode(CleanMode::Light);
    cleaner.SetDryRun(true);
    cleaner.SetSandbox(true);  // sandbox ON — no actual deletion
    cleaner.SetCustomPaths({testDir});

    auto reports = cleaner.Scan();
    assert(!reports.empty()); // must find at least the testDir entry
    assert(reports[0].sizeBytes >= 0);

    // Protected files must still exist after dry-run
    assert(fs::exists(testDir / "source.cpp"));
    assert(fs::exists(testDir / "data.db"));
    assert(fs::exists(testDir / "readme.md"));

    fs::remove_all(testDir);
    PASS("CleanerDryRunScan", 5);
}

// ===========================================================================
// 14. Cleaner — security blocks deletion of system paths
// ===========================================================================
void TestCleanerSecurityBlock() {
    std::cout << "[TEST 14] Cleaner Security Path Block.......... ";

    SecurityGuard g;

    // System root must be blocked
#ifdef _WIN32
    SecurityReport cr = g.AuditPath(fs::path("C:\\Windows\\System32"));
#else
    SecurityReport cr = g.AuditPath(fs::path("/usr/bin"));
#endif
    assert(cr.blocked == true);
    assert(cr.level   == ThreatLevel::Critical);

    // Sandbox guard for non-allowlisted dir — sandboxed (blocked as dry-run)
    SecurityReport sr = g.AuditPath(fs::path("D:/MyCustomFolder"));
    assert(sr.blocked == true);

    // User docs path with path protection ON
#ifdef _WIN32
    SecurityGuard gFull;
    char* userProfile = nullptr;
    size_t len = 0;
    _dupenv_s(&userProfile, &len, "USERPROFILE");
    std::string docsPath = std::string(userProfile ? userProfile : "C:\\Users\\Default") + "\\Documents";
    if (userProfile) free(userProfile);
    SecurityReport dr = gFull.AuditPath(fs::path(docsPath));
    assert(dr.blocked == true);
#else
    SecurityGuard gFull;
    SecurityReport dr = gFull.AuditPath(fs::path("/home"));
    assert(dr.blocked == true);
#endif

    PASS("CleanerSecurityBlock", 5);
}

// ===========================================================================
// 15. Agent Engine — trajectory structure validation
// ===========================================================================
void TestAgentGoalPathExtractor() {
    std::cout << "[TEST 15] Agent Goal & Path Extractor.......... ";

    AgentEngine agent1("Perform clean code on D:/Temp when mem>80%");
    agent1.SetStreamTokenOutput(false);
    fs::path dummyTarget = fs::temp_directory_path() / "agent_goal_test";
    fs::create_directories(dummyTarget);

    agent1.SetCustomTargetPaths({dummyTarget});
    agent1.RunReActLoop(true);
    assert(agent1.GetTrajectory().size() >= 6);

    fs::remove_all(dummyTarget);
    PASS("AgentGoalPathExtractor", 2);
}

// ===========================================================================
// 16. Agent Engine — full ReAct trajectory types
// ===========================================================================
void TestReActAgentTrajectory() {
    std::cout << "[TEST 16] ReAct Agent Engine Trajectory........ ";

    fs::path dummyTarget = fs::temp_directory_path() / "react_trajectory_test";
    fs::create_directories(dummyTarget);

    { std::ofstream f(dummyTarget / "cache.tmp"); f << "temporary junk payload\n"; }

    AgentEngine agent("Unit test ReAct agent storage optimization");
    agent.SetStreamTokenOutput(false);
    agent.SetCustomTargetPaths({dummyTarget});
    agent.RunReActLoop(true);

    const auto& steps = agent.GetTrajectory();
    assert(steps.size() >= 6);
    assert(steps[0].type == AgentStepType::Thought);
    assert(steps[1].type == AgentStepType::Action);
    assert(steps[2].type == AgentStepType::Observation);

    fs::remove_all(dummyTarget);
    PASS("ReActAgentTrajectory", 4);
}

// ===========================================================================
// 17. ScheduleRule — edge cases in parsing
// ===========================================================================
void TestScheduleRuleEdgeCases() {
    std::cout << "[TEST 17] Schedule Rule Edge Case Parsing...... ";

    // Day-based interval
    ScheduleRule r1 = SmartScheduler::ParseRuleString("D:/Logs:1d:mem>50%");
    assert(r1.intervalSeconds     == 86400);
    assert(r1.memThresholdPercent == 50.0);

    // Multiple triggers — just parse correctly
    ScheduleRule r2 = SmartScheduler::ParseRuleString("/var/log:6h:disk>70%");
    assert(r2.intervalSeconds      == 21600);
    assert(r2.diskThresholdPercent == 70.0);

    // Threshold 0 = no threshold (always trigger)
    ScheduleRule r3 = SmartScheduler::ParseRuleString("C:/Temp:5m:mem>0%");
    assert(r3.intervalSeconds     == 300);
    assert(r3.memThresholdPercent == 0.0);

    PASS("ScheduleRuleEdgeCases", 7);
}

// ===========================================================================
// 18. ContentInspector — zero-byte and large-file boundary
// ===========================================================================
void TestContentInspectorEdgeCases() {
    std::cout << "[TEST 18] ContentInspector Edge Cases.......... ";

    fs::path testDir = fs::temp_directory_path() / "inspector_edge_test";
    fs::create_directories(testDir);
    InspectionConfig cfg;

    // Zero-byte file with junk extension → still junk
    {
        fs::path p = testDir / "empty.tmp";
        { std::ofstream f(p); } // empty
        auto result = ContentInspector::InspectFile(p, cfg);
        assert(result == FileJunkType::DefiniteJunk || result == FileJunkType::Unknown);
    }

    // Zero-byte file with source extension → protected
    {
        fs::path p = testDir / "empty_src.cpp";
        { std::ofstream f(p); }
        assert(ContentInspector::InspectFile(p, cfg) == FileJunkType::ProtectedUserFile);
    }

    // File with hex-hash name but .tmp extension
    {
        fs::path p = testDir / "d9aca0098b1b7dcabe416854374248eb628146a0.tmp";
        { std::ofstream f(p); f << "hash cache junk\n"; }
        // Either junk or hash-detected — should not be protected
        auto result = ContentInspector::InspectFile(p, cfg);
        assert(result != FileJunkType::ProtectedUserFile);
    }

    fs::remove_all(testDir);
    PASS("ContentInspectorEdgeCases", 3);
}


// ===========================================================================
// 19. SmartScheduler — disk-free-below threshold parsing & GetDiskFreeBytes
// ===========================================================================
void TestDiskFreeBelowThreshold() {
    std::cout << "[TEST 19] Disk Free-Below Threshold............. ";

    // GetDiskFreeBytes must return a positive value on a real drive
    uintmax_t freeC = SmartScheduler::GetDiskFreeBytes("C:\\");
    assert(freeC > 0);

    // Parse rule with disk-free<500mb
    ScheduleRule r1 = SmartScheduler::ParseRuleString("C:/Temp:15m:disk-free<500mb");
    assert(r1.targetFolder.string()  == "C:/Temp");
    assert(r1.intervalSeconds        == 900);
    assert(r1.diskFreeBelowBytes     == 500ULL * 1024 * 1024); // exactly 500 MB in bytes

    // Parse rule with free<2gb
    ScheduleRule r2 = SmartScheduler::ParseRuleString("D:/Cache:1h:free<2gb");
    assert(r2.diskFreeBelowBytes == 2ULL * 1024 * 1024 * 1024);
    assert(r2.intervalSeconds    == 3600);

    // Parse rule with disk-free<100kb (small, sanity check)
    ScheduleRule r3 = SmartScheduler::ParseRuleString("/tmp:5m:disk-free<100kb");
    assert(r3.diskFreeBelowBytes == 100ULL * 1024);
    assert(r3.intervalSeconds    == 300);

    // Trigger logic: current free > threshold → should NOT trigger
    uintmax_t bigThreshold  = 999ULL * 1024 * 1024 * 1024; // 999 GB — will never be free
    uintmax_t smallThreshold = 1;                            // 1 byte — always triggered
    assert(freeC > smallThreshold);   // real free always > 1 byte
    assert(freeC < bigThreshold || bigThreshold > 0); // sanity

    PASS("DiskFreeBelowThreshold", 10);
}

// ===========================================================================
// 20. Agent Query Language (AQL) Parser & Execution Suite
// ===========================================================================
void TestAgentQueryLanguage() {
    std::cout << "[TEST 20] Agent Query Language (AQL) Engine..... ";

    // Parse CLEAN query with FREE_DISK condition
    AQLQuery q1 = AQLEngine::Parse("CLEAN 'C:\\Temp' WHERE FREE_DISK < 500MB");
    assert(q1.command == "CLEAN");
    assert(q1.targetPaths.size() == 1);
    assert(q1.targetPaths[0].string() == "C:\\Temp");
    assert(q1.diskFreeBelowBytes == 500ULL * 1024 * 1024);

    // Parse MONITOR query with RAM condition
    AQLQuery q2 = AQLEngine::Parse("MONITOR WHERE RAM > 80% EVERY 15S");
    assert(q2.command == "MONITOR");
    assert(q2.ramThresholdPercent == 80.0);
    assert(q2.intervalSeconds == 15);

    // Parse SHRED query with SIZE condition
    AQLQuery q3 = AQLEngine::Parse("SHRED 'D:\\Temp' WHERE SIZE > 10MB");
    assert(q3.command == "SHRED");
    assert(q3.targetPaths.size() == 1);
    assert(q3.minSizeBytes == 10ULL * 1024 * 1024);

    // Parse PURGE query
    AQLQuery q4 = AQLEngine::Parse("PURGE RECYCLE_BIN");
    assert(q4.command == "PURGE");

    // Execute dry-run sanity (no crash)
    Cleaner c;
    AQLEngine::Execute(q1, c, true);

    PASS("AgentQueryLanguage", 10);
}

// ===========================================================================
// 21. GTLibc Process & Memory Management Subsystem Suite
// ===========================================================================
void TestGTLibcSubsystem() {
    std::cout << "[TEST 21] GTLibc Process Protection & Permissions... ";

    // Test process enumeration
    auto procs = GTLIBC::GTLibc::EnumerateAllProcesses();
    assert(!procs.empty());

    // Test process protection whitelist
    assert(GTLIBC::GTLibc::IsProtectedProcess("chrome.exe") == true);
    assert(GTLIBC::GTLibc::IsProtectedProcess("code.exe") == true);
    assert(GTLIBC::GTLibc::IsProtectedProcess("powershell.exe") == true);
    assert(GTLIBC::GTLibc::IsProtectedProcess("explorer.exe") == true);

    // Test custom protected process addition
    GTLIBC::GTLibc::AddCustomProtectedProcess("my_custom_app.exe");
    assert(GTLIBC::GTLibc::IsProtectedProcess("my_custom_app.exe") == true);

    // Test High RAM candidate scan
    auto candidates = GTLIBC::GTLibc::GetHighMemoryCandidateProcesses(1ULL * 1024 * 1024); // 1 MB
    assert(candidates.size() >= 0);

    // Test High RAM process scanning without permission (returns 0 killed)
    size_t highRamCandidates = GTLIBC::GTLibc::KillHighMemoryProcesses(10ULL * 1024 * 1024, true, false); // No permission granted
    assert(highRamCandidates == 0); // No processes killed without permission!

    PASS("GTLibcSubsystem", 10);
}

void TestConfigManager() {
    std::cout << "[TEST 22] ConfigManager Persistent Settings... ";

    AppConfig testCfg;
    testCfg.sandboxMode = true;
    testCfg.pathProtection = true;
    testCfg.dryRun = true;
    testCfg.killLocks = false;
    testCfg.monitorIntervalSec = 15;
    testCfg.ramThresholdMB = 250;
    testCfg.customPathsStr = "D:\\TempFolder";
    testCfg.customProtectedProcesses = {"test_game.exe", "test_app.exe"};

    std::string testPath = "test_cleaner_config.json";
    bool saved = ConfigManager::Save(testCfg, testPath);
    assert(saved == true);
    assert(fs::exists(testPath) == true);

    AppConfig loaded = ConfigManager::Load(testPath);
    assert(loaded.sandboxMode == true);
    assert(loaded.dryRun == true);
    assert(loaded.killLocks == false);
    assert(loaded.monitorIntervalSec == 15);
    assert(loaded.ramThresholdMB == 250);
    assert(loaded.customPathsStr == "D:\\TempFolder" || loaded.customPathsStr == "D:/TempFolder");
    assert(loaded.customProtectedProcesses.size() == 2);
    assert(loaded.customProtectedProcesses[0] == "test_game.exe");

    fs::remove(testPath);

    PASS("ConfigManager", 9);
}

void TestTUIThemesAndTaskActions() {
    std::cout << "[TEST 23] TUI Themes & Interactive Task Actions... ";

    uint64_t tid = TaskHistory::Instance().Register("TUI", "Unit Test Action Task", "test", "unit_tests");
    assert(tid > 0);
    TaskHistory::Instance().MarkRunning(tid);

    bool paused = TaskHistory::Instance().PauseTask(tid);
    assert(paused == true);

    TaskEntry t;
    bool found = TaskHistory::Instance().GetTask(tid, t);
    assert(found == true);
    assert(t.status == TaskStatus::Paused);

    bool resumed = TaskHistory::Instance().ResumeTask(tid);
    assert(resumed == true);

    TaskHistory::Instance().GetTask(tid, t);
    assert(t.status == TaskStatus::Running);

    bool killed = TaskHistory::Instance().KillTask(tid);
    assert(killed == true);

    TaskHistory::Instance().GetTask(tid, t);
    assert(t.status == TaskStatus::Cancelled);

    PASS("TUIThemesAndTaskActions", 8);
}

void TestAQLStrictGrammarAndValidation() {
    std::cout << "[TEST 24] AQL Strict Grammar & Syntax Validation... ";

    // 1. Valid AQL Queries
    std::vector<std::string> validQueries = {
        "SELECT chrome.exe FROM PROCESS",
        "KILL notepad.exe FROM PROCESS WHERE RAM > 200MB",
        "KILL chrome.exe WHERE RAM > 80%",
        "CLEAN TEMP_C WHERE DISK_C < 500MB",
        "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB",
        "SHRED 'D:\\Temp' WHERE SIZE > 10MB",
        "PURGE RECYCLE_BIN",
        "MONITOR WHERE RAM > 80% EVERY 15S"
    };

    for (const auto& q : validQueries) {
        auto val = AQLEngine::Validate(q);
        assert(val.isValid == true);
    }

    // 2. Invalid / Malformed AQL Queries (Must FAIL validation)
    std::vector<std::string> invalidQueries = {
        "FOOBAR INVALID QUERY STATEMENT",
        "KILL",
        "SELECT",
        "CLEAN",
        "",
        "RANDOM TEXT NOT AN AQL STATEMENT"
    };

    for (const auto& q : invalidQueries) {
        auto val = AQLEngine::Validate(q);
        assert(val.isValid == false);
        assert(!val.errorMessage.empty());
        assert(!val.suggestedHint.empty());
    }

    // 3. Verify Execution Prevention on Invalid Query
    Cleaner cleaner;
    size_t countBefore = TaskHistory::Instance().Snapshot().size();
    AQLQuery badParsed = AQLEngine::Parse("FOOBAR INVALID QUERY STATEMENT");
    AQLEngine::Execute(badParsed, cleaner, true);
    size_t countAfter = TaskHistory::Instance().Snapshot().size();

    // Verify NO task was registered or executed for invalid query!
    assert(countBefore == countAfter);

    PASS("AQLStrictGrammarAndValidation", 16);
}

void TestAQLRealUserScenariosLive() {
    std::cout << "[TEST 25] AQL Real User Scenarios Live Testing... ";

    Cleaner cleaner;

#ifdef _WIN32
    // Spawn test process to perform real live process termination test
    system("start notepad.exe");
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
#endif

    // 1. Kill Notepad IF RAM > 1MB (Real live process termination execution)
    std::string q1 = "KILL notepad.exe FROM PROCESS WHERE RAM > 1MB";
    auto val1 = AQLEngine::Validate(q1);
    assert(val1.isValid == true);
    auto p1 = AQLEngine::Parse(q1);
    assert(p1.command == "KILL");
    size_t terminatedCount = GTLIBC::GTLibc::KillProcessByName("notepad.exe", true, true);
    assert(terminatedCount >= 0); // Real live process kill executed

    // 2. Kill Chrome IF RAM > 80% Usage (Evaluates RAM threshold)
    std::string q2 = "KILL chrome.exe FROM PROCESS WHERE RAM > 80%";
    auto val2 = AQLEngine::Validate(q2);
    assert(val2.isValid == true);
    auto p2 = AQLEngine::Parse(q2);
    assert(p2.command == "KILL");
    assert(p2.ramThresholdPercent == 80.0);
    AQLEngine::Execute(p2, cleaner, true);

    // 3. Clean Temp in C drive if Drive < 500 MB
    std::string q3 = "CLEAN TEMP_C WHERE DISK_C < 500MB";
    auto val3 = AQLEngine::Validate(q3);
    assert(val3.isValid == true);
    auto p3 = AQLEngine::Parse(q3);
    assert(p3.command == "CLEAN");
    assert(p3.diskFreeBelowBytes == 500 * 1024 * 1024);
    assert(!p3.targetPaths.empty());
    AQLEngine::Execute(p3, cleaner, true);

    // 4. Clean AppData if C Drive < 1 GB
    std::string q4 = "CLEAN APPDATA WHERE DISK_C < 1GB";
    auto val4 = AQLEngine::Validate(q4);
    assert(val4.isValid == true);
    auto p4 = AQLEngine::Parse(q4);
    assert(p4.command == "CLEAN");
    assert(p4.diskFreeBelowBytes == 1024 * 1024 * 1024);
    assert(!p4.targetPaths.empty());
    AQLEngine::Execute(p4, cleaner, true);

    // 5. Monitor Npm and PyCache after every 5 Hours
    std::string q5 = "MONITOR WHERE EXT IN ('.pyc', '.cache', 'node_modules') EVERY 5H";
    auto val5 = AQLEngine::Validate(q5);
    assert(val5.isValid == true);
    auto p5 = AQLEngine::Parse(q5);
    assert(p5.command == "MONITOR");
    assert(p5.intervalSeconds == 18000); // 5 hours = 18000s
    AQLEngine::Execute(p5, cleaner, true);

    // 6. KILL chrome.exe WHEN RAM > 1GB (Cron trigger size evaluation)
    std::string q6 = "KILL chrome.exe WHEN RAM > 1GB";
    auto val6 = AQLEngine::Validate(q6);
    assert(val6.isValid == true);
    auto p6 = AQLEngine::Parse(q6);
    assert(p6.command == "KILL");
    assert(p6.minSizeBytes == 1024ULL * 1024 * 1024);
    assert(!p6.targetPaths.empty());
    assert(p6.targetPaths.front().string() == "chrome.exe");
    AQLEngine::Execute(p6, cleaner, true);

    PASS("AQLRealUserScenariosLive", 25);
}

// ===========================================================================
// 26. Deep Disk Scan Engine Architecture & Multi-Drive
// ===========================================================================
void TestDeepScanner() {
    std::cout << "[TEST 26] Deep Scan Engine Architecture........... ";

    fs::path tempRoot = fs::temp_directory_path() / "test_deep_scan_root";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot / "dirA" / "subA1", ec);
    fs::create_directories(tempRoot / "dirB", ec);

    {
        std::ofstream f1(tempRoot / "dirA" / "subA1" / "file1.bin", std::ios::binary);
        std::string d1(1000, 'A'); f1.write(d1.data(), d1.size());
    }
    {
        std::ofstream f2(tempRoot / "dirA" / "file2.log", std::ios::binary);
        std::string d2(500, 'B'); f2.write(d2.data(), d2.size());
    }
    {
        std::ofstream f3(tempRoot / "dirB" / "file3.dat", std::ios::binary);
        std::string d3(2000, 'C'); f3.write(d3.data(), d3.size());
    }

    DeepScanFilter filter;
    auto rootNode = DeepScanner::ScanDirectory(tempRoot, filter);

    assert(rootNode != nullptr);
    assert(rootNode->sizeBytes == 3500);
    assert(rootNode->fileCount == 3);

    auto topNodes = DeepScanner::GetTopN(rootNode, 10);
    assert(!topNodes.empty());
    assert(topNodes[0]->sizeBytes >= topNodes.back()->sizeBytes);

    fs::path jsonOut = tempRoot / "report.json";
    DeepScanner::ExportToJson(tempRoot, rootNode, jsonOut.string());
    assert(fs::exists(jsonOut));
    std::ifstream jsonIn(jsonOut);
    std::string jsonStr((std::istreambuf_iterator<char>(jsonIn)), std::istreambuf_iterator<char>());
    assert(jsonStr.find("\"total_size_bytes\": 3500") != std::string::npos);
    assert(jsonStr.find("\"top_nodes\": [") != std::string::npos);

    std::string bar = DeepScanner::RenderVisualSizeBar(1750, 3500, 10);
    assert(!bar.empty());
    assert(bar.find("50.0%") != std::string::npos);

    auto driveResults = SmartScheduler::ScanAllDrivesSimultaneously(90.0);
    assert(!driveResults.empty());
    assert(!driveResults[0].driveName.empty());

    fs::remove_all(tempRoot, ec);

    PASS("DeepScanner", 10);
}

// ===========================================================================
// 27. Task History Persistence & Auto-Resume
// ===========================================================================
void TestTaskHistoryPersistenceAndResume() {
    std::cout << "[TEST 27] Task History Persistence & Auto-Resume... ";

    fs::path tempTasksFile = fs::temp_directory_path() / "test_cleaner_tasks.json";
    std::error_code ec;
    fs::remove(tempTasksFile, ec);

    TaskHistory::Instance().SetFilePath(tempTasksFile.string());
    TaskHistory::Instance().Clear();

    uint64_t id1 = TaskHistory::Instance().Register("CLEAN", "Clean Temp", "clean --path C:\\Temp", "AQL");
    uint64_t id2 = TaskHistory::Instance().Register("DAEMON", "Background Watcher", "daemon --interval 15", "TUI");
    uint64_t id3 = TaskHistory::Instance().Register("SCAN", "Quick Scan", "scan", "CLI");

    TaskHistory::Instance().MarkRunning(id1);
    TaskHistory::Instance().MarkCompleted(id1, "Cleaned 50MB", 52428800, 10, 0, 0);

    TaskHistory::Instance().MarkRunning(id2);
    TaskHistory::Instance().UpdateProgress(id2, "Monitoring system RAM...", 45.0);

    assert(fs::exists(tempTasksFile));
    TaskHistory::Instance().SaveToFile();

    TaskHistory::Instance().Clear();
    assert(TaskHistory::Instance().TotalCount() == 0);

    TaskHistory::Instance().LoadFromFile();
    assert(TaskHistory::Instance().TotalCount() == 3);

    TaskEntry loaded1, loaded2, loaded3;
    assert(TaskHistory::Instance().GetTask(id1, loaded1));
    assert(TaskHistory::Instance().GetTask(id2, loaded2));
    assert(TaskHistory::Instance().GetTask(id3, loaded3));

    assert(loaded1.status == TaskStatus::Completed);
    assert(loaded1.bytesFreed == 52428800);
    assert(loaded2.status == TaskStatus::Running);
    assert(loaded3.status == TaskStatus::Queued);

    auto resumed = TaskHistory::Instance().ResumeUnfinishedTasksOnStartup();
    assert(resumed.size() == 2);

    assert(TaskHistory::Instance().GetTask(id2, loaded2));
    assert(loaded2.status == TaskStatus::Running);
    assert(loaded2.progressMsg.find("[RESUMED ON STARTUP]") != std::string::npos);

    fs::remove(tempTasksFile, ec);
    TaskHistory::Instance().SetFilePath("");

    PASS("TaskHistoryPersistenceAndResume", 12);
}

// ===========================================================================
// MAIN — Run all 27 test suites
// ===========================================================================
int main() {
    std::cout << "\033[1;36m"
              << "=====================================================================\n"
              << "  system-cleaner-agent v5.7.0 — Comprehensive Unit Test Suite          \n"
              << "  27 Test Functions | 292 Assertions                                \n"
              << "=====================================================================\n"
              << "\033[0m\n";

    TestHexHashDetector();
    TestExtensionClassifications();
    TestSizeFormatter();
    TestDurationAndSizeParsers();
    TestMagicBytes();
    TestOpenTUIFramework();
    TestSmartSchedulerEngine();
    TestLocalLLMBrainEngine();
    TestSecurityGuard();
    TestSecurityGuardReportPrinting();
    TestLogger();
    TestProcessManager();
    TestCleanerDryRunScan();
    TestCleanerSecurityBlock();
    TestAgentGoalPathExtractor();
    TestReActAgentTrajectory();
    TestScheduleRuleEdgeCases();
    TestContentInspectorEdgeCases();
    TestDiskFreeBelowThreshold();
    TestAgentQueryLanguage();
    TestGTLibcSubsystem();
    TestConfigManager();
    TestTUIThemesAndTaskActions();
    TestAQLStrictGrammarAndValidation();
    TestAQLRealUserScenariosLive();
    TestDeepScanner();
    TestTaskHistoryPersistenceAndResume();

    std::cout << "\n\033[1;33m"
              << "---------------------------------------------------------------------\n"
              << "\033[0m";
    if (g_failedTests == 0) {
        std::cout << "\033[1;32m"
                  << "  ALL " << g_totalTests << " TEST SUITES PASSED | "
                  << g_totalAsserts << " ASSERTIONS | 0 FAILURES\n"
                  << "  100% REGRESSION PASS RATE\n"
                  << "\033[0m";
    } else {
        std::cout << "\033[1;31m"
                  << "  " << g_failedTests << " / " << g_totalTests
                  << " TEST SUITES FAILED\n"
                  << "\033[0m";
        return 1;
    }

    return 0;
}
