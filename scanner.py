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
