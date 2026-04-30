# Design: Raw Content Fetching & Regex Verification

## Goal
Implement a verification step that fetches the raw content of a file found on GitHub and verifies the presence of the `sk-ant-` pattern using regular expressions.

## Proposed Changes

### 1. Imports
- Add `import re` to `scanner.py`.

### 2. `verify_match(raw_url, token)` Function
- **Inputs**: `raw_url` (string), `token` (string).
- **Behavior**:
    - Fetch content from `raw_url` using `requests.get()`.
    - Use `Authorization: Bearer <token>` header.
    - Set `timeout=10`.
    - If status code is not 200, return `None, None`.
    - If status code is 200, use regex `sk-ant-[a-zA-Z0-9]{11,}` to find all matches.
    - Return `(matches, content)` if matches found, else `(None, None)`.
- **Error Handling**: Catch and print any exceptions during the fetch.

## Verification Strategy
- **Unit Testing**: Create a test script that mocks `requests.get` to return content with and without the pattern.
- **Manual Verification**: Run the script (once integrated in later tasks) against a known raw URL.

## Security Considerations
- Ensure the `GITHUB_TOKEN` is used securely in the `Bearer` token header.
- The regex pattern `sk-ant-[a-zA-Z0-9]{11,}` is specific to Anthropic API keys.
