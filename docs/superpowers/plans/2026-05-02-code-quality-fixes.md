# Code Quality Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Address critical code quality issues identified in Task 2, including security, robustness, and project hygiene.

**Architecture:** 
1. Improve GitHub API authentication by switching to the modern Bearer token format and adding timeouts.
2. Enhance error reporting for authentication failures.
3. Establish a standard `.gitignore` to prevent leakage of sensitive data and build artifacts.

**Tech Stack:** Python, `requests` library.

---

### Task 1: Project Hygiene - .gitignore

**Files:**
- Create: `.gitignore`

- [ ] **Step 1: Create .gitignore file**

Write the following content to `.gitignore`:
```text
.env
__pycache__/
*.csv
build/
*.exe
*.log
```

- [ ] **Step 2: Verify .gitignore exists**

Run: `ls -a .gitignore`
Expected: File exists.

- [ ] **Step 3: Commit**

```bash
git add .gitignore
git commit -m "chore: add .gitignore"
```

---

### Task 2: GitHub API Client Improvements

**Files:**
- Modify: `scanner.py`

- [ ] **Step 1: Update check_auth with timeout, Bearer token, and specific error messaging**

```python
def check_auth(token):
    """Verifies the GitHub Personal Access Token."""
    # Use Bearer token format
    headers = {"Authorization": f"Bearer {token}"}
    try:
        # Add timeout=10
        response = requests.get("https://api.github.com/user", headers=headers, timeout=10)
        
        if response.status_code == 401:
            print("Error: Invalid GITHUB_TOKEN (401 Unauthorized). Please check your credentials.")
            sys.exit(1)
        elif response.status_code != 200:
            print(f"Error: GitHub API returned status code {response.status_code}")
            sys.exit(1)
            
        print("Authentication successful.")
    except requests.exceptions.Timeout:
        print("Error: Connection to GitHub timed out.")
        sys.exit(1)
    except Exception as e:
        print(f"Error connecting to GitHub: {e}")
        sys.exit(1)
```

- [ ] **Step 2: Verify changes in scanner.py**

Run: `cat scanner.py` and manually verify the `check_auth` implementation.

- [ ] **Step 3: Commit**

```bash
git add scanner.py
git commit -m "refactor: improve GitHub authentication and error handling"
```
