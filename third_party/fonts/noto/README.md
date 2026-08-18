# Noto runtime font cache

Run `bash tools/fetch_ui_fonts_mingw64.sh` from Git Bash at the repository root.
The wrapper invokes Windows PowerShell, downloads the manifest-pinned upstream
font binaries and records SHA-256 values in `font-lock.json`.

The downloaded `.ttf/.otf/.woff2` files are machine-local build inputs and are
**not** meant to be committed. The Git Bash wrapper adds those binary patterns
to this checkout's private `.git/info/exclude` instead of modifying the
repository's existing `.gitignore`.

`font-lock.json` is intentionally **not** excluded. Review and commit it after a
font refresh so a release can be audited against exact upstream URLs, file sizes
and SHA-256 hashes without storing roughly a full global font cache in ordinary
Git history.
