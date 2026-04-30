"""GitHub Secret Scanner - Global search for sk-ant- patterns."""
import os
import sys
import argparse
import re
import requests
import time
import csv
from dotenv import load_dotenv

def check_auth(token):
    """Verifies the GitHub Personal Access Token."""
    headers = {"Authorization": f"Bearer {token}"}
    try:
        response = requests.get("https://api.github.com/user", headers=headers, timeout=10)
        if response.status_code == 401:
            print("Error: 401 Unauthorized. Please check if your GITHUB_TOKEN is correct and has the required scopes.")
            sys.exit(1)
        elif response.status_code != 200:
            print(f"Error: Invalid GITHUB_TOKEN (Status: {response.status_code})")
            sys.exit(1)
        print("Authentication successful.")
    except requests.exceptions.Timeout:
        print("Error: Connection to GitHub timed out (10s limit).")
        sys.exit(1)
    except Exception as e:
        print(f"Error connecting to GitHub: {e}")
        sys.exit(1)

def search_github(token, query, page=1, max_retries=3):
    """Searches GitHub for code matching the query with a retry loop."""
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github.v3+json"
    }
    url = f"https://api.github.com/search/code?q={query}&page={page}"
    
    retries = 0
    while retries < max_retries:
        try:
            print(f"Searching GitHub (Query: {query}, Page: {page}, Attempt: {retries+1})...")
            response = requests.get(url, headers=headers, timeout=10)
            
            if response.status_code == 403:
                print("Rate limit hit or secondary limit detected. Sleeping for 60s...")
                time.sleep(60)
                retries += 1
                continue
                
            response.raise_for_status()
            return response.json()
            
        except requests.exceptions.Timeout:
            print(f"Error: Search timed out for page {page}. Retrying...")
            retries += 1
            time.sleep(5)
        except requests.exceptions.RequestException as e:
            print(f"Error during search (Page {page}): {e}")
            return None
            
    print(f"Error: Maximum retries reached for page {page}.")
    return None

def verify_match(raw_url, token):
    """Fetches raw content and verifies the pattern using regex with size limits."""
    headers = {"Authorization": f"Bearer {token}"}
    try:
        # Use stream=True to check headers before downloading body
        resp = requests.get(raw_url, headers=headers, timeout=10, stream=True)
        
        if resp.status_code != 200:
            print(f"Warning: Failed to fetch raw content from {raw_url} (Status: {resp.status_code})")
            return None, None
            
        # Check file size (Content-Length)
        file_size = int(resp.headers.get("Content-Length", 0))
        if file_size > 1024 * 1024:  # 1MB limit
            print(f"Skipping large file: {raw_url} ({file_size} bytes)")
            return None, None
            
        # Note: If Content-Length is missing (chunked encoding), we might still read a large file.
        # But for GitHub raw content, it's usually present.
        content = resp.text
        # Pattern: sk-ant- followed by 11 or more alphanumeric chars
        # \b ensures we match the start of the token correctly.
        pattern = re.compile(r"\bsk-ant-[a-zA-Z0-9]{11,}")
        matches = pattern.findall(content)
        
        if matches:
            return matches, content
        return None, None
    except Exception as e:
        print(f"Error fetching raw content from {raw_url}: {e}")
        return None, None

def save_result(repo, path, match, line, url):
    """Saves a finding to the CSV file."""
    file_exists = os.path.isfile("github_scan_results.csv")
    try:
        with open("github_scan_results.csv", "a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(["Repository", "Path", "Match", "Line", "URL"])
            writer.writerow([repo, path, match, line, url])
    except Exception as e:
        print(f"Error saving result to CSV: {e}")

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

    query = f'"{args.pattern}"'
    results = search_github(token, query)
    
    if not results or "items" not in results:
        print("No results found or error occurred.")
        return

    print(f"Found {len(results['items'])} potential files. Verifying...")
    
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
        
        # Stay under 10 requests/minute (Search API + Raw Content Fetch)
        time.sleep(6)

if __name__ == "__main__":
    main()
