@echo off
setlocal

set "BAZEL_OUTPUT_USER_ROOT=%~dp0bazel-user-root"

call "%~dp0scripts\bazel.cmd" --output_user_root=%BAZEL_OUTPUT_USER_ROOT% test --repo_contents_cache= //... --verbose_failures
exit /b %ERRORLEVEL%
