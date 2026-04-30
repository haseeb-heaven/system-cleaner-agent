# Design Document: GitHub Secret Scanner

**Date:** 2026-05-01
**Topic:** GitHub Secret Scanner using Search API
**Status:** Approved

## 1. Overview
A Python-based security research tool to identify specific code patterns (`sk-ant-`) across public GitHub repositories using the GitHub Search API.

## 2. Architecture
The scanner is a standalone Python CLI script.

### 2.1 Components
- **API Client:** Manages requests to GitHub API (`/search/code` and `raw.githubusercontent.com`).
- **Rate Limiter:** Implements a strict 6-second delay between search calls and handles 429/secondary limit headers.
- **Pattern Matcher:** Uses Regex (`sk-ant-[a-zA-Z0-9]{11,}`) on raw file content to verify matches and ensure length constraints (>10 chars after prefix).
- **Result Processor:** Formats and saves findings to a CSV file.

## 3. Data Flow
1. Authenticate using `GITHUB_TOKEN`.
2. Query `/search/code?q="sk-ant-"`.
3. For each result:
    a. Fetch raw file content.
    b. Apply Regex to find the exact match.
    c. Verify total length > 10.
    d. Extract line content and repository metadata.
4. Append row to `github_scan_results.csv`.
5. Wait 6 seconds before next pagination or search request.

## 4. Constraints & Safety
- **Rate Limit:** 10 requests per minute (authenticated).
- **Result Limit:** GitHub API caps code search at 1,000 results per query.
- **Security:** Token is read from an environment variable.
- **Privacy:** Output matches are logged with minimal context to prevent accidental exposure of full secrets in insecure logs (though the primary goal is discovery).

## 5. Success Criteria
- Script runs without hitting secondary rate limits.
- Findings are correctly filtered by length (>10 characters).
- Results are saved in a valid CSV table format.
