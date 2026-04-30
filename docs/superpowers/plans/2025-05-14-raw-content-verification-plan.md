# Raw Content Fetching & Regex Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `verify_match` function in `scanner.py` to fetch raw GitHub content and verify patterns with regex.

**Architecture:** Add a standalone verification function that uses `requests` and `re`.

**Tech Stack:** Python 3, `requests`, `re`.

---

### Task 1: Setup Imports and Regex Function in `scanner.py`

**Files:**
- Modify: `D:\Software\gemini-sys-cleaner\scanner.py`

- [ ] **Step 1: Add `import re` and `verify_match` function**

```python
import re

def verify_match(raw_url, token):
    """Fetches raw content and verifies the pattern using regex."""
    headers = {"Authorization": f"Bearer {token}"}
    try:
        resp = requests.get(raw_url, headers=headers, timeout=10)
        if resp.status_code != 200:
            return None, None
        
        content = resp.text
        # Pattern: sk-ant- followed by 11 or more alphanumeric chars
        # This ensures the length after prefix is > 10.
        pattern = re.compile(r"sk-ant-[a-zA-Z0-9]{11,}")
        matches = pattern.findall(content)
        
        if matches:
            return matches, content
        return None, None
    except Exception as e:
        print(f"Error fetching raw content from {raw_url}: {e}")
        return None, None
```

- [ ] **Step 2: Commit initial changes**

```bash
git add scanner.py
git commit -m "feat: add raw content verification with regex"
```

### Task 2: Verify `verify_match` with Tests

**Files:**
- Create: `D:\Software\gemini-sys-cleaner\tests\test_scanner.py`

- [ ] **Step 1: Write a test for `verify_match`**

```python
import unittest
from unittest.mock import patch, MagicMock
from scanner import verify_match

class TestScanner(unittest.TestCase):
    @patch('requests.get')
    def test_verify_match_success(self, mock_get):
        # Mock successful response with matches
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.text = "Here is a secret: sk-ant-12345678901"
        mock_get.return_value = mock_response
        
        matches, content = verify_match("https://raw.githubusercontent.com/user/repo/main/file.txt", "fake-token")
        
        self.assertEqual(matches, ["sk-ant-12345678901"])
        self.assertEqual(content, mock_response.text)

    @patch('requests.get')
    def test_verify_match_no_pattern(self, mock_get):
        # Mock successful response without matches
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.text = "No secrets here."
        mock_get.return_value = mock_response
        
        matches, content = verify_match("https://raw.githubusercontent.com/user/repo/main/file.txt", "fake-token")
        
        self.assertIsNone(matches)
        self.assertIsNone(content)

    @patch('requests.get')
    def test_verify_match_404(self, mock_get):
        # Mock 404 response
        mock_response = MagicMock()
        mock_response.status_code = 404
        mock_get.return_value = mock_response
        
        matches, content = verify_match("https://raw.githubusercontent.com/user/repo/main/file.txt", "fake-token")
        
        self.assertIsNone(matches)
        self.assertIsNone(content)

if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run the tests**

Run: `python -m unittest tests/test_scanner.py`
Expected: 3 tests pass.

- [ ] **Step 3: Final Commit**

```bash
git add tests/test_scanner.py
git commit -m "test: add tests for verify_match"
```
