import os
import sys
import shutil
import time
import ctypes
from pathlib import Path
from collections import defaultdict

def is_reparse_point(path_str):
    """Check if path is a reparse point (junction/symlink) on Windows to avoid loops/access issues."""
    try:
        # 0x400 is FILE_ATTRIBUTE_REPARSE_POINT
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

class SystemCleaner:
    def __init__(self, target_drive="C:\\"):
        self.target_drive = target_drive
        self.large_files = []
        self.folder_sizes = defaultdict(int)
        self.folder_subdirs = defaultdict(list)
        self.scanned_files_count = 0
        self.scanned_folders_count = 0
        self.errors_count = 0
        
        # Define common cache and temp directories to clean
        user_profile = os.environ.get("USERPROFILE", "")
        system_root = os.environ.get("SystemRoot", "C:\\Windows")
        local_appdata = os.environ.get("LOCALAPPDATA", "")
        appdata = os.environ.get("APPDATA", "")
        program_data = os.environ.get("ProgramData", "C:\\ProgramData")
        
        self.clean_targets = {
            "User Temp Folder": [os.environ.get("TEMP", "")],
            "System Temp Folder": [os.path.join(system_root, "Temp")],
            "Windows Prefetch": [os.path.join(system_root, "Prefetch")],
            "Windows Software Distribution Download": [os.path.join(system_root, "SoftwareDistribution", "Download")],
            "Windows Error Reporting Archive": [os.path.join(program_data, r"Microsoft\Windows\WER\ReportArchive")] if program_data else [],
            "Windows Error Reporting Queue": [os.path.join(program_data, r"Microsoft\Windows\WER\ReportQueue")] if program_data else [],
            "Crash Dumps": [os.path.join(local_appdata, "CrashDumps")] if local_appdata else [],
            "DirectX Shader Cache": [os.path.join(local_appdata, "D3DSCache")] if local_appdata else [],
            "Google Chrome Cache": [
                os.path.join(local_appdata, r"Google\Chrome\User Data\Default\Cache"),
                os.path.join(local_appdata, r"Google\Chrome\User Data\Default\Code Cache")
            ] if local_appdata else [],
            "Microsoft Edge Cache": [
                os.path.join(local_appdata, r"Microsoft\Edge\User Data\Default\Cache"),
                os.path.join(local_appdata, r"Microsoft\Edge\User Data\Default\Code Cache")
            ] if local_appdata else [],
            "Spotify Cache": [os.path.join(local_appdata, r"Spotify\Storage")] if local_appdata else [],
            "Discord Cache": [
                os.path.join(appdata, r"discord\Cache"),
                os.path.join(appdata, r"discord\Code Cache")
            ] if appdata else [],
            "npm Cache": [os.path.join(local_appdata, "npm-cache"), os.path.join(user_profile, ".npm")] if local_appdata and user_profile else [],
            "pip Cache": [os.path.join(local_appdata, r"pip\Cache")] if local_appdata else [],
            "uv Cache": [os.path.join(local_appdata, r"uv\cache")] if local_appdata else [],
            "Yarn Cache": [os.path.join(local_appdata, r"Yarn\Cache")] if local_appdata else [],
            "Gradle Cache": [os.path.join(user_profile, r".gradle\caches")] if user_profile else [],
            "NuGet Cache": [os.path.join(user_profile, r".nuget\packages")] if user_profile else [],
            "Cargo Cache": [os.path.join(user_profile, r".cargo\registry\cache")] if user_profile else [],
            "Go Build Cache": [os.path.join(local_appdata, "go-build")] if local_appdata else [],
            "Project build/dist/node_modules directories (Common Cache)": []
        }

    def scan_drive(self):
        """Scans the C: drive for files/folders > 100 MB."""
        print(f"Starting drive scan on {self.target_drive}...")
        print("Note: Skipping system junctions, symlinks, and restricted folders to avoid permission issues.")
        print("Scanning in progress...")
        
        start_time = time.time()
        limit_size = 100 * 1024 * 1024 # 100 MB
        
        # We will walk the directory tree manually to handle reparse points and errors cleanly
        self._scan_directory(self.target_drive, limit_size)
        
        elapsed = time.time() - start_time
        print(f"\nScan completed in {elapsed:.2f} seconds.")
        print(f"Scanned {self.scanned_files_count:,} files and {self.scanned_folders_count:,} folders.")
        print(f"Encountered {self.errors_count:,} inaccessible locations (skipped).")

    def _scan_directory(self, current_dir, limit_size):
        """Recursive directory scanner with safety checks."""
        self.scanned_folders_count += 1
        
        # Status update every 20,000 files/folders
        total_scanned = self.scanned_files_count + self.scanned_folders_count
        if total_scanned % 20000 == 0:
            print(f"  Processed {total_scanned:,} items... (Found {len(self.large_files)} large files > 100MB)")

        try:
            with os.scandir(current_dir) as it:
                for entry in it:
                    try:
                        # Skip symlinks and junctions
                        if entry.is_symlink() or is_reparse_point(entry.path):
                            continue
                            
                        if entry.is_file():
                            self.scanned_files_count += 1
                            try:
                                size = entry.stat().st_size
                                # Record if file > 100MB
                                if size >= limit_size:
                                    self.large_files.append((entry.path, size))
                                
                                # Propagate size to parents
                                path_parts = Path(entry.path).parents
                                for parent in path_parts:
                                    parent_str = str(parent)
                                    self.folder_sizes[parent_str] += size
                            except Exception:
                                pass
                                
                        elif entry.is_dir():
                            # We don't walk into specific system folders that are locked/reparse points or massive OS system dirs
                            # to keep the scan times reasonable and avoid useless noise.
                            dirname = entry.name.lower()
                            if dirname in ["$recycle.bin", "system volume information", "$winre_backup_partition.marker", "windows"]:
                                # Skip system volume and recycle bin, but we can search Windows folder separately or ignore it
                                # to prevent listing Windows system DLLs.
                                continue
                            
                            parent_str = str(Path(entry.path).parent)
                            self.folder_subdirs[parent_str].append(entry.path)
                            self._scan_directory(entry.path, limit_size)
                            
                    except PermissionError:
                        self.errors_count += 1
                    except Exception:
                        self.errors_count += 1
        except PermissionError:
            self.errors_count += 1
        except Exception:
            self.errors_count += 1

    def get_large_folders(self, limit_size=100 * 1024 * 1024):
        """Extracts hotspots: folders > 100 MB where size is not dominated (>80%) by a single subfolder."""
        hotspots = []
        for folder, size in self.folder_sizes.items():
            if size < limit_size:
                continue
                
            subdirs = self.folder_subdirs.get(folder, [])
            if not subdirs:
                # Leaf folder with large file(s)
                hotspots.append((folder, size))
                continue
                
            # Check if there is a dominant subfolder taking > 80% of the size
            max_sub_size = 0
            for sd in subdirs:
                sd_size = self.folder_sizes.get(sd, 0)
                if sd_size > max_sub_size:
                    max_sub_size = sd_size
            
            if max_sub_size < 0.8 * size:
                hotspots.append((folder, size))
                
        return sorted(hotspots, key=lambda x: x[1], reverse=True)

    def print_scan_report(self):
        """Prints a detailed report of the scan results."""
        limit_size = 100 * 1024 * 1024
        
        # 1. Large Files Report
        print("\n" + "="*80)
        print("                    LARGE FILES REPORT (> 100 MB)")
        print("="*80)
        sorted_files = sorted(self.large_files, key=lambda x: x[1], reverse=True)
        if not sorted_files:
            print("No files larger than 100 MB found.")
        else:
            print(f"{'File Path':<65} | {'Size':<10}")
            print("-"*80)
            for path, size in sorted_files[:50]:  # Limit to top 50 to avoid clutter
                # Shorten path if too long
                display_path = path if len(path) <= 65 else "..." + path[-62:]
                print(f"{display_path:<65} | {format_size(size):<10}")
            if len(sorted_files) > 50:
                print(f"... and {len(sorted_files) - 50} more large files.")
                
        # 2. Large Folders (Hotspots) Report
        print("\n" + "="*80)
        print("                  LARGE FOLDER HOTSPOTS REPORT (> 100 MB)")
        print("="*80)
        print("A 'hotspot' is a directory that contains significant data, either directly")
        print("or split across multiple subfolders (not just dominated by a single subfolder).")
        print("-"*80)
        hotspots = self.get_large_folders(limit_size)
        if not hotspots:
            print("No folder hotspots larger than 100 MB found.")
        else:
            print(f"{'Folder Path':<65} | {'Total Size':<10}")
            print("-"*80)
            for path, size in hotspots[:50]:  # Limit to top 50
                display_path = path if len(path) <= 65 else "..." + path[-62:]
                print(f"{display_path:<65} | {format_size(size):<10}")
            if len(hotspots) > 50:
                print(f"... and {len(hotspots) - 50} more folder hotspots.")

    def scan_caches(self):
        """Scans the designated temp/cache directories and reports their sizes."""
        print("\n" + "="*80)
        print("                    CACHE & TEMP DIRECTORIES SCAN")
        print("="*80)
        
        total_cache_size = 0
        target_sizes = {}
        
        for name, paths in self.clean_targets.items():
            if not paths:
                continue
            name_size = 0
            for path in paths:
                if os.path.exists(path):
                    name_size += self._get_dir_size(path)
            target_sizes[name] = name_size
            total_cache_size += name_size
            
        # Display sizes
        for name, size in sorted(target_sizes.items(), key=lambda x: x[1], reverse=True):
            print(f"{name:<55} : {format_size(size)}")
            
        print("-"*80)
        print(f"Total space that can be freed from caches: {format_size(total_cache_size)}")
        return total_cache_size

    def _get_dir_size(self, path):
        """Get total size of a directory in bytes."""
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

    def clean_caches(self):
        """Cleans the designated temp/cache directories."""
        print("\n" + "="*80)
        print("                    STARTING CACHE & TEMP CLEANUP")
        print("="*80)
        
        total_freed = 0
        
        for name, paths in self.clean_targets.items():
            if not paths:
                continue
            print(f"Cleaning {name}...")
            freed_for_target = 0
            for path in paths:
                if os.path.exists(path):
                    freed_for_target += self._delete_path_contents(path)
            print(f"  Freed: {format_size(freed_for_target)}")
            total_freed += freed_for_target
            
        print("-"*80)
        print(f"Cleanup completed. Total space freed: {format_size(total_freed)}")
        return total_freed

    def _delete_path_contents(self, path):
        """Deletes files and folders inside a path, keeping the root if it is a main directory."""
        freed = 0
        if os.path.isfile(path):
            try:
                size = os.path.getsize(path)
                os.remove(path)
                return size
            except Exception as e:
                print(f"    Failed to remove file {path}: {e}")
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
                            # verify if deleted
                            if not os.path.exists(entry.path):
                                freed += dir_size
                    except PermissionError:
                        # Skip files locked by processes
                        pass
                    except Exception as e:
                        pass
        except Exception as e:
            print(f"    Error accessing {path}: {e}")
        return freed

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Gemini System Cleaner & Drive Space Analyzer")
    parser.add_argument("--scan", action="store_true", help="Scan C: drive for files/folders > 100MB and cache sizes")
    parser.add_argument("--clean", action="store_true", help="Clean C: drive temp and cache folders")
    args = parser.parse_args()
    
    # Check for admin rights to clean system temp/prefetch
    is_admin = False
    try:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception:
        pass
        
    if not is_admin:
        print("WARNING: Script is not running as Administrator. Some system folders (System Temp, Prefetch, etc.) may fail to scan or clean due to permissions.\n")
        
    cleaner = SystemCleaner()
    
    if args.scan:
        cleaner.scan_caches()
        cleaner.scan_drive()
        cleaner.print_scan_report()
    elif args.clean:
        cleaner.clean_caches()
    else:
        # Run scan by default if no arguments provided
        cleaner.scan_caches()
        print("\nTo scan the entire C: drive for large files/folders (> 100 MB), run:")
        print("  python sys_cleaner.py --scan")
        print("\nTo clean cache and temp files, run:")
        print("  python sys_cleaner.py --clean")
