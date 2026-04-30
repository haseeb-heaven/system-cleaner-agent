# GitHub Secret Scanner Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a Python-based security scanner that finds `sk-ant-` patterns globally on GitHub while respecting free-tier rate limits.

**Architecture:** A standalone Python script using `requests` to query GitHub's Search API, fetch raw content for verification, and save results to a CSV.

**Tech Stack:** Python 3, `requests` library.

---

### Task 1: Environment & Boilerplate

**Files:**
- Create: `scanner.py`
- Create: `requirements.txt`
- Create: `.env.example`

- [ ] **Step 1: Create requirements.txt**
```text
requests
python-dotenv
```

- [ ] **Step 2: Create .env.example**
```text
GITHUB_TOKEN=your_token_here
```

- [ ] **Step 3: Create scanner.py boilerplate with basic argument parsing**
```python
import os
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(description="GitHub Secret Scanner")
    parser.add_argument("--pattern", default="sk-ant-", help="Pattern to search for")
    args = parser.parse_args()
    print(f"Starting scan for pattern: {args.pattern}")

if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Commit**
```bash
git add scanner.py requirements.txt .env.example
git commit -m "chore: initial scanner boilerplate"
```

---

### Task 2: GitHub API Client & Authentication

**Files:**
- Modify: `scanner.py`

- [ ] **Step 1: Add authentication check**
```python
import requests

def check_auth(token):
    headers = {"Authorization": f"token {token}"}
    response = requests.get("https://api.github.com/user", headers=headers)
    if response.status_code != 200:
        print("Error: Invalid GITHUB_TOKEN")
        sys.exit(1)
    print("Authentication successful.")
```

- [ ] **Step 2: Update main to read token from environment**
```python
from dotenv import load_dotenv

def main():
    load_dotenv()
    token = os.getenv("GITHUB_TOKEN")
    if not token:
        print("Error: GITHUB_TOKEN not found in environment")
        sys.exit(1)
    
    check_auth(token)
    # ... rest of main ...
```

- [ ] **Step 3: Commit**
```bash
git add scanner.py
git commit -m "feat: add github authentication check"
```

---

### Task 3: Search Logic & Rate Limiting

**Files:**
- Modify: `scanner.py`

- [ ] **Step 1: Implement search function with 6-second delay**
```python
import time

def search_github(token, query, page=1):
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github.v3+json"
    }
    url = f"https://api.github.com/search/code?q={query}&page={page}"
    print(f"Searching page {page}...")
    response = requests.get(url, headers=headers)
    
    if response.status_code == 403:
        print("Rate limit hit or secondary limit. Sleeping for 60s...")
        time.sleep(60)
        return search_github(token, query, page)
        
    return response.json()
```

- [ ] **Step 2: Commit**
```bash
git add scanner.py
git commit -m "feat: add github search logic with rate limiting"
```

---

### Task 4: Raw Content Fetching & Regex Verification

**Files:**
- Modify: `scanner.py`

- [ ] **Step 1: Add regex verification function**
```python
import re

def verify_match(raw_url, token):
    # Fetch raw content
    headers = {"Authorization": f"token {token}"}
    resp = requests.get(raw_url, headers=headers)
    if resp.status_code != 200:
        return None
    
    content = resp.text
    # Pattern: sk-ant- followed by 11 or more alphanumeric chars
    pattern = re.compile(r"sk-ant-[a-zA-Z0-9]{11,}")
    matches = pattern.findall(content)
    
    if matches:
        return matches, content
    return None, None
```

- [ ] **Step 2: Commit**
```bash
git add scanner.py
git commit -m "feat: add raw content verification with regex"
```

---

### Task 5: CSV Output & Orchestration

**Files:**
- Modify: `scanner.py`

- [ ] **Step 1: Implement CSV writing and main loop**
```python
import csv

def save_result(repo, path, match, line, url):
    file_exists = os.path.isfile("github_scan_results.csv")
    with open("github_scan_results.csv", "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        if not file_exists:
            writer.writerow(["Repository", "Path", "Match", "Line", "URL"])
        writer.writerow([repo, path, match, line, url])

def main():
    # ... previous setup ...
    query = f'"{args.pattern}"'
    results = search_github(token, query)
    
    for item in results.get("items", []):
        repo_full_name = item["repository"]["full_name"]
        file_path = item["path"]
        html_url = item["html_url"]
        
        # Construct raw URL (simplified for public repos)
        raw_url = html_url.replace("github.com", "raw.githubusercontent.com").replace("/blob/", "/")
        
        matches, content = verify_match(raw_url, token)
        if matches:
            for m in matches:
                # Find the line containing the match
                lines = content.splitlines()
                match_line = next((l for l in lines if m in l), "N/A").strip()
                save_result(repo_full_name, file_path, m, match_line, html_url)
                print(f"Found match: {m} in {repo_full_name}")
        
        time.sleep(6) # Strict delay between processing items or pages
```

- [ ] **Step 2: Commit**
```bash
git add scanner.py
git commit -m "feat: complete scanner with csv output"
```
