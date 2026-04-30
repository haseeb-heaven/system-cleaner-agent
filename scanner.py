"""GitHub Secret Scanner - Global search for sk-ant- patterns."""
import os
import sys
import argparse
import requests
from dotenv import load_dotenv

def check_auth(token):
    """Verifies the GitHub Personal Access Token."""
    headers = {"Authorization": f"token {token}"}
    try:
        response = requests.get("https://api.github.com/user", headers=headers)
        if response.status_code != 200:
            print(f"Error: Invalid GITHUB_TOKEN (Status: {response.status_code})")
            sys.exit(1)
        print("Authentication successful.")
    except Exception as e:
        print(f"Error connecting to GitHub: {e}")
        sys.exit(1)

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
