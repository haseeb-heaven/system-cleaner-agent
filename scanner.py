"""GitHub Secret Scanner - Global search for sk-ant- patterns."""
import os
import sys
import argparse
import requests
import time
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

def search_github(token, query, page=1):
    """Searches GitHub for code matching the query."""
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github.v3+json"
    }
    url = f"https://api.github.com/search/code?q={query}&page={page}"
    print(f"Searching GitHub (Page {page})...")
    
    try:
        response = requests.get(url, headers=headers, timeout=10)
        
        if response.status_code == 403:
            # Handle rate limiting or secondary limits
            print("Rate limit hit or secondary limit detected. Sleeping for 60s...")
            time.sleep(60)
            return search_github(token, query, page)
            
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        print(f"HTTP Error: {e}")
        return {}
    except Exception as e:
        print(f"Error during search: {e}")
        return {}

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

if __name__ == "__main__":
    main()
