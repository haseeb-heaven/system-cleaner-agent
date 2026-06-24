# Design Doc: Scanner Pagination and Rate-Limiting Optimization

## Goal
Implement pagination (up to 10 pages) in the GitHub Secret Scanner and optimize the rate-limiting logic to be more efficient while staying within GitHub's API limits.

## Requirements
1. **Pagination**: Iterate through pages 1-10 of search results.
2. **Optimized Sleep**: 
    - 1s delay between individual file fetches.
    - 6s delay between page searches.
3. **Robustness**: Gracefully handle failed page fetches by continuing or breaking.
4. **Summary**: Report the total number of items processed.

## Implementation Details

### `main()` Refactoring
The `main()` function will be updated to use a `while` loop:
- Initialize `page = 1` and `total_processed = 0`.
- Call `search_github(token, query, page=page)`.
- If results are found:
    - Process each item with a 1s sleep.
    - Increment `total_processed`.
- After processing all items on a page:
    - Increment `page`.
    - If `page <= 10`, sleep for 6s before the next search.
- Print a summary of `total_processed`.

### Logic for `main()`
```python
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
            # ... verify_match and save_result ...
            time.sleep(1) 
            total_processed += 1
            
        page += 1
        if page <= 10:
            print(f"Waiting 6 seconds before next search request...")
            time.sleep(6)
```

## Verification Plan
1. **Manual Run**: Run the scanner and observe the logs for pagination and wait times.
2. **CSV Check**: Ensure results are correctly appended to `github_scan_results.csv`.
3. **API Monitoring**: Verify that no "403 Forbidden" (rate limit) errors occur with the new timings.
