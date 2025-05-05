@echo off
REM Squash entire Git history, preserving submodules and remotes

REM Step 1: Ensure we are in a Git repo
if not exist ".git" (
    echo Not a Git repository.
    exit /b 1
)

REM Step 2: Get current branch name
FOR /F "tokens=*" %%i IN ('git rev-parse --abbrev-ref HEAD') DO set CURRENT_BRANCH=%%i

REM Step 3: Create orphan branch (no history)
git checkout --orphan temp-clean-branch

REM Step 4: Add all files and submodules
git add -A

REM Step 5: Commit as new initial commit
git commit -m "init"

REM Step 6: Delete old branch and rename new one
git branch -D %CURRENT_BRANCH%
git branch -m %CURRENT_BRANCH%

REM Step 7: Clean up unnecessary objects
git gc --aggressive --prune=now

REM Step 8: Show current status
echo.
echo Done! Repository history squashed to one commit.
git log --oneline -n 1
echo.
echo NOTE: You must force-push to remote if you've already pushed before:
echo     git push --force
