# Scanner Pagination and Rate-Limiting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement pagination (up to 10 pages) and optimize rate-limiting delays in `scanner.py`.

**Architecture:** Refactor `main()` to use a `while` loop for pagination, adjusting sleep intervals to 1s between file fetches and 6s between page search requests.

**Tech Stack:** Python, `requests`, `time`.

---

### Task 1: Refactor `main()` for Pagination and Rate Limiting

**Files:**
- Modify: `D:\Software\gemini-sys-cleaner\scanner.py`

- [ ] **Step 1: Update `main()` with pagination loop**

Replace the single search call with a `while` loop that iterates from page 1 to 10.

```python
def main():
    """Main entry point for the scanner."""
    load_dotenv()
    token = os.getenv("GITHUB_TOKEN")
    
    parser = argparse.ArgumentParser(description="GitHub Secret Scanner")
    parser.add_argument("--pattern", default="sk-ant-", help="Pattern to search for")
    args = parser.parse_args()
    
    if not token:
        print("Error: GITHUB_TOKEN not found in environment. Create a .env file.")
        sys.exit(1)
        
    check_auth(token)
    print(f"Starting scan for pattern: {args.pattern}")

    page = 1
    total_processed = 0
    query = f'"{args.pattern}"'
    
    while page <= 10:
        results = search_github(token, query, page=page)
        
        if not results or "items" not in results or not results["items"]:
            print(f"No more results or error at page {page}.")
            break

        print(f"Processing page {page} ({len(results['items'])} items)...")
        
        for item in results.get("items", []):
            repo_full_name = item["repository"]["full_name"]
            file_path = item["path"]
            html_url = item["html_url"]
            
            # Construct raw URL
            raw_url = html_url.replace("github.com", "raw.githubusercontent.com").replace("/blob/", "/")
            
            matches, content = verify_match(raw_url, token)
            if matches:
                lines = content.splitlines()
                for m in matches:
                    # Find the first line containing the match
                    match_line = next((l for l in lines if m in l), "N/A").strip()
                    save_result(repo_full_name, file_path, m, match_line, html_url)
                    print(f"Found match: {m} in {repo_full_name}")
            
            total_processed += 1
            # Smaller delay for raw fetch
            time.sleep(1)

        page += 1
        if page <= 10:
            print(f"Waiting 6 seconds before next search request...")
            time.sleep(6) # Delay between Search API calls

    print(f"\nScan complete. Total files processed: {total_processed}")
```

- [ ] **Step 2: Verify the change**

Run: `python scanner.py --pattern "sk-ant-"` (assuming GITHUB_TOKEN is set)
Expected: Logs show processing page 1, then waiting 6 seconds, then page 2 (if exists), etc.

- [ ] **Step 3: Commit the changes**

Run: `git add scanner.py`
Run: `git commit -m "refactor: add pagination and optimize rate limiting"`
