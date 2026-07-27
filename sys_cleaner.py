import os
import sys
import shutil
import time
import ctypes
import argparse
from pathlib import Path
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

def is_reparse_point(path_str):
    """Check if path is a reparse point (junction/symlink) on Windows to avoid loops/access issues."""
    try:
        return bool(os.stat(path_str).st_file_attributes & 0x400)
    except Exception:
        return False

def format_size(size_bytes):
    """Format size into human readable string."""
    if size_bytes < 1024:
        return f"{size_bytes} B"
    elif size_bytes < 1024 * 1024:
        return f"{size_bytes / 1024:.2f} KB"
    elif size_bytes < 1024 * 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.2f} MB"
    else:
        return f"{size_bytes / (1024 * 1024 * 1024):.2f} GB"

def get_available_drives():
    """Detect available fixed drives on Windows."""
    drives = []
    if sys.platform == "win32":
        bitmask = ctypes.windll.kernel32.GetLogicalDrives()
        for letter in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
            if bitmask & 1:
                drive_path = f"{letter}:\\"
                if os.path.exists(drive_path):
                    drives.append(drive_path)
            bitmask >>= 1
    else:
        drives.append("/")
    return drives

def empty_recycle_bin():
    """Empty Windows Recycle Bin via Shell32 API."""
    if sys.platform == "win32":
        try:
            # Flags: 7 = SHERB_NOCONFIRMATION (1) | SHERB_NOPROGRESSUI (2) | SHERB_NOSOUND (4)
            flags = 7
            result = ctypes.windll.shell32.SHEmptyRecycleBinW(None, None, flags)
            if result == 0:
                print("Successfully emptied Windows Recycle Bin.")
                return True
            else:
                print("Recycle Bin was already empty or could not be emptied.")
                return False
        except Exception as e:
            print(f"Error emptying Recycle Bin: {e}")
            return False
    return False

class SystemCleaner:
    def __init__(self, target_drives=None):
        if target_drives is None:
            target_drives = ["C:\\"]
        self.target_drives = target_drives
        self.large_files = []
        self.folder_sizes = defaultdict(int)
        self.folder_subdirs = defaultdict(list)
        self.scanned_files_count = 0
        self.scanned_folders_count = 0
        self.errors_count = 0
        
        # Comprehensive system, user, app, browser, and developer cache targets
        user_profile = os.environ.get("USERPROFILE", "")
        system_root = os.environ.get("SystemRoot", "C:\\Windows")
        local_appdata = os.environ.get("LOCALAPPDATA", "")
        appdata = os.environ.get("APPDATA", "")
        program_data = os.environ.get("ProgramData", "C:\\ProgramData")
        temp_dir = os.environ.get("TEMP", "")
        
        self.clean_targets = {
            "User Temp Folder": [temp_dir, os.path.join(user_profile, r"AppData\Local\Temp")] if user_profile else [],
            "System Temp Folder": [os.path.join(system_root, "Temp"), "D:\\tmp"],
            "Windows Prefetch": [os.path.join(system_root, "Prefetch")],
            "Windows SoftwareDistribution Download": [os.path.join(system_root, r"SoftwareDistribution\Download")],
            "Windows Error Reporting Archive": [os.path.join(program_data, r"Microsoft\Windows\WER\ReportArchive")] if program_data else [],
            "Windows Error Reporting Queue": [os.path.join(program_data, r"Microsoft\Windows\WER\ReportQueue")] if program_data else [],
            "Windows Crash Dumps": [os.path.join(local_appdata, "CrashDumps"), os.path.join(system_root, "Minidump")] if local_appdata else [],
            "DirectX Shader Cache": [os.path.join(local_appdata, "D3DSCache")] if local_appdata else [],
            "Google Chrome Cache": [
                os.path.join(local_appdata, r"Google\Chrome\User Data\Default\Cache"),
                os.path.join(local_appdata, r"Google\Chrome\User Data\Default\Code Cache")
            ] if local_appdata else [],
            "Microsoft Edge Cache": [
                os.path.join(local_appdata, r"Microsoft\Edge\User Data\Default\Cache"),
                os.path.join(local_appdata, r"Microsoft\Edge\User Data\Default\Code Cache")
            ] if local_appdata else [],
            "Brave Browser Cache": [
                os.path.join(local_appdata, r"BraveSoftware\Brave-Browser\User Data\Default\Cache")
            ] if local_appdata else [],
            "Firefox Cache": [os.path.join(local_appdata, r"Mozilla\Firefox\Profiles")] if local_appdata else [],
            "Spotify Cache": [os.path.join(local_appdata, r"Spotify\Storage")] if local_appdata else [],
            "Discord Cache": [
                os.path.join(appdata, r"discord\Cache"),
                os.path.join(appdata, r"discord\Code Cache")
            ] if appdata else [],
            "Telegram Cache": [os.path.join(appdata, r"Telegram Desktop\tdata\user_data")] if appdata else [],
            "Slack Cache": [os.path.join(appdata, r"Slack\Cache")] if appdata else [],
            "npm & Node Cache": [
                os.path.join(local_appdata, "npm-cache"),
                os.path.join(user_profile, ".npm"),
                "D:\\npm-cache"
            ] if user_profile else [],
            "pip & Python Cache": [
                os.path.join(local_appdata, r"pip\Cache"),
                os.path.join(user_profile, r".cache\pip")
            ] if local_appdata else [],
            "uv Cache": [os.path.join(local_appdata, r"uv\cache")] if local_appdata else [],
            "Yarn & pnpm Cache": [
                os.path.join(local_appdata, r"Yarn\Cache"),
                os.path.join(local_appdata, r"pnpm\store")
            ] if local_appdata else [],
            "Gradle & Maven Cache": [
                os.path.join(user_profile, r".gradle\caches"),
                os.path.join(user_profile, r".m2\repository")
            ] if user_profile else [],
            "NuGet Cache": [os.path.join(user_profile, r".nuget\packages")] if user_profile else [],
            "Cargo & Rust Cache": [
                os.path.join(user_profile, r".cargo\registry\cache"),
                os.path.join(user_profile, r".cargo\git\db")
            ] if user_profile else [],
            "Go Build Cache": [os.path.join(local_appdata, "go-build")] if local_appdata else [],
            "VS Code & Cursor Cache": [
                os.path.join(appdata, r"Code\Cache"),
                os.path.join(appdata, r"Code\CachedData"),
                os.path.join(appdata, r"Cursor\Cache")
            ] if appdata else []
        }

    def scan_drive_parallel(self, threshold_mb=100):
        """Scans the configured drive(s) using multi-threading for speed."""
        limit_size = threshold_mb * 1024 * 1024
        start_time = time.time()
        
        for drive in self.target_drives:
            print(f"\nStarting parallel drive scan on {drive} (Threshold: >{threshold_mb}MB)...")
            if not os.path.exists(drive):
                print(f"Drive {drive} does not exist. Skipping.")
                continue

            # Gather top-level directories for parallel processing
            top_items = []
            try:
                with os.scandir(drive) as it:
                    for entry in it:
                        if entry.is_dir() and not entry.is_symlink() and not is_reparse_point(entry.path):
                            dirname = entry.name.lower()
                            if dirname not in ["$recycle.bin", "system volume information", "$winre_backup_partition.marker", "windows"]:
                                top_items.append(entry.path)
            except Exception as e:
                print(f"Error accessing root drive {drive}: {e}")
                continue

            with ThreadPoolExecutor(max_workers=8) as executor:
                futures = {executor.submit(self._scan_directory_recursive, item, limit_size): item for item in top_items}
                for future in as_completed(futures):
                    try:
                        future.result()
                    except Exception:
                        pass

        elapsed = time.time() - start_time
        print(f"\nScan completed in {elapsed:.2f} seconds.")
        print(f"Scanned {self.scanned_files_count:,} files and {self.scanned_folders_count:,} folders.")
        print(f"Encountered {self.errors_count:,} inaccessible items (skipped).")

    def _scan_directory_recursive(self, current_dir, limit_size):
        """Recursive directory scanner."""
        self.scanned_folders_count += 1
        
        try:
            with os.scandir(current_dir) as it:
                for entry in it:
                    try:
                        if entry.is_symlink() or is_reparse_point(entry.path):
                            continue
                            
                        if entry.is_file():
                            self.scanned_files_count += 1
                            try:
                                size = entry.stat().st_size
                                if size >= limit_size:
                                    self.large_files.append((entry.path, size))
                                
                                path_parts = Path(entry.path).parents
                                for parent in path_parts:
                                    parent_str = str(parent)
                                    self.folder_sizes[parent_str] += size
                            except Exception:
                                pass
                                
                        elif entry.is_dir():
                            dirname = entry.name.lower()
                            if dirname in ["$recycle.bin", "system volume information", "$winre_backup_partition.marker", "windows"]:
                                continue
                            
                            parent_str = str(Path(entry.path).parent)
                            self.folder_subdirs[parent_str].append(entry.path)
                            self._scan_directory_recursive(entry.path, limit_size)
                            
                    except Exception:
                        self.errors_count += 1
        except Exception:
            self.errors_count += 1

    def get_large_folders(self, limit_size=100 * 1024 * 1024):
        """Extracts folder hotspots where size is not dominated (>80%) by a single subfolder."""
        hotspots = []
        for folder, size in self.folder_sizes.items():
            if size < limit_size:
                continue
                
            subdirs = self.folder_subdirs.get(folder, [])
            if not subdirs:
                hotspots.append((folder, size))
                continue
                
            max_sub_size = max((self.folder_sizes.get(sd, 0) for sd in subdirs), default=0)
            if max_sub_size < 0.8 * size:
                hotspots.append((folder, size))
                
        return sorted(hotspots, key=lambda x: x[1], reverse=True)

    def print_scan_report(self, threshold_mb=100):
        """Prints formatted report of scan findings."""
        limit_size = threshold_mb * 1024 * 1024
        
        print("\n" + "="*80)
        print(f"                    LARGE FILES REPORT (> {threshold_mb} MB)")
        print("="*80)
        sorted_files = sorted(self.large_files, key=lambda x: x[1], reverse=True)
        if not sorted_files:
            print(f"No files larger than {threshold_mb} MB found.")
        else:
            print(f"{'File Path':<65} | {'Size':<10}")
            print("-"*80)
            for path, size in sorted_files[:30]:
                display_path = path if len(path) <= 65 else "..." + path[-62:]
                print(f"{display_path:<65} | {format_size(size):<10}")
            if len(sorted_files) > 30:
                print(f"... and {len(sorted_files) - 30} more large files.")
                
        print("\n" + "="*80)
        print(f"                  LARGE FOLDER HOTSPOTS (> {threshold_mb} MB)")
        print("="*80)
        hotspots = self.get_large_folders(limit_size)
        if not hotspots:
            print(f"No folder hotspots larger than {threshold_mb} MB found.")
        else:
            print(f"{'Folder Path':<65} | {'Total Size':<10}")
            print("-"*80)
            for path, size in hotspots[:30]:
                display_path = path if len(path) <= 65 else "..." + path[-62:]
                print(f"{display_path:<65} | {format_size(size):<10}")
            if len(hotspots) > 30:
                print(f"... and {len(hotspots) - 30} more folder hotspots.")

    def scan_caches(self):
        """Scans all designated temp/cache directories and reports sizes."""
        print("\n" + "="*80)
        print("                    CACHE & TEMP DIRECTORIES SCAN")
        print("="*80)
        
        total_cache_size = 0
        target_sizes = {}
        
        for name, paths in self.clean_targets.items():
            if not paths:
                continue
            name_size = sum(self._get_dir_size(p) for p in paths if os.path.exists(p))
            if name_size > 0:
                target_sizes[name] = name_size
                total_cache_size += name_size
            
        for name, size in sorted(target_sizes.items(), key=lambda x: x[1], reverse=True):
            print(f"{name:<55} : {format_size(size)}")
            
        print("-"*80)
        print(f"Total cleanable space across cache locations: {format_size(total_cache_size)}")
        return total_cache_size

    def _get_dir_size(self, path):
        """Calculates total size of a path in bytes."""
        total = 0
        try:
            if os.path.isfile(path):
                return os.path.getsize(path)
            for root, dirs, files in os.walk(path):
                for f in files:
                    fp = os.path.join(root, f)
                    if not os.path.islink(fp):
                        try:
                            total += os.path.getsize(fp)
                        except Exception:
                            pass
        except Exception:
            pass
        return total

    def clean_caches(self, dry_run=False):
        """Cleans all designated temp/cache directories."""
        print("\n" + "="*80)
        mode_str = "[DRY-RUN PREVIEW]" if dry_run else "[EXECUTING CLEANUP]"
        print(f"                    CACHE & TEMP CLEANUP {mode_str}")
        print("="*80)
        
        total_freed = 0
        
        for name, paths in self.clean_targets.items():
            if not paths:
                continue
            freed_for_target = 0
            for path in paths:
                if os.path.exists(path):
                    if dry_run:
                        freed_for_target += self._get_dir_size(path)
                    else:
                        freed_for_target += self._delete_path_contents(path)
            if freed_for_target > 0:
                print(f"Cleaning {name:<50} : Freed {format_size(freed_for_target)}")
                total_freed += freed_for_target
            
        print("-"*80)
        action_verb = "Would free" if dry_run else "Total space freed"
        print(f"Cleanup finished. {action_verb}: {format_size(total_freed)}")
        return total_freed

    def _delete_path_contents(self, path):
        """Deletes files and folders inside a path safely."""
        freed = 0
        if os.path.isfile(path):
            try:
                size = os.path.getsize(path)
                os.remove(path)
                return size
            except Exception:
                return 0
                
        try:
            with os.scandir(path) as it:
                for entry in it:
                    try:
                        if entry.is_symlink() or is_reparse_point(entry.path):
                            os.remove(entry.path)
                            continue
                            
                        if entry.is_file():
                            size = entry.stat().st_size
                            os.remove(entry.path)
                            freed += size
                        elif entry.is_dir():
                            dir_size = self._get_dir_size(entry.path)
                            shutil.rmtree(entry.path, ignore_errors=True)
                            if not os.path.exists(entry.path):
                                freed += dir_size
                    except Exception:
                        pass
        except Exception:
            pass
        return freed

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Gemini System Cleaner & Drive Space Analyzer v2.0")
    parser.add_argument("--scan", action="store_true", help="Scan drives for files/folders > threshold MB and analyze caches")
    parser.add_argument("--clean", action="store_true", help="Clean temp, cache folders, and optionally empty Recycle Bin")
    parser.add_argument("--dry-run", action="store_true", help="Preview space that can be freed without deleting anything")
    parser.add_argument("--recycle-bin", action="store_true", help="Empty Windows Recycle Bin")
    parser.add_argument("--drive", default="C:\\", help="Target drive letter to scan (e.g., C:\\, D:\\, or 'all')")
    parser.add_argument("--threshold", type=int, default=100, help="Threshold size in MB for large files/folders report (default: 100)")
    args = parser.parse_args()
    
    # Check admin privileges
    is_admin = False
    try:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception:
        pass
        
    if not is_admin:
        print("WARNING: Script is not running as Administrator. Admin privileges are recommended to access all system temp folders.")

    # Determine drives to scan
    if args.drive.lower() == "all":
        target_drives = get_available_drives()
    else:
        target_drives = [args.drive if args.drive.endswith("\\") else args.drive + "\\"]

    cleaner = SystemCleaner(target_drives=target_drives)
    
    if args.recycle_bin:
        empty_recycle_bin()

    if args.scan:
        cleaner.scan_caches()
        cleaner.scan_drive_parallel(threshold_mb=args.threshold)
        cleaner.print_scan_report(threshold_mb=args.threshold)
    elif args.clean or args.dry_run:
        cleaner.scan_caches()
        cleaner.clean_caches(dry_run=args.dry_run)
    else:
        # Default behavior: show cache summary and available options
        cleaner.scan_caches()
        print("\nUsage Options:")
        print("  python sys_cleaner.py --scan                  # Full scan of drives and cache locations")
        print("  python sys_cleaner.py --scan --drive D:\\      # Scan D: drive specifically")
        print("  python sys_cleaner.py --scan --drive all       # Scan all available drives")
        print("  python sys_cleaner.py --clean                 # Clean all cache and temp folders")
        print("  python sys_cleaner.py --clean --recycle-bin   # Clean caches AND empty Windows Recycle Bin")
        print("  python sys_cleaner.py --dry-run               # Preview cleanup space without deleting")
