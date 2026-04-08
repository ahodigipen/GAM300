"""
upload_installer.py
Usage:
    python upload_installer.py github <tag>   e.g. python upload_installer.py github v0.0.1
    python upload_installer.py dropbox
"""

import sys
import os

# ========================
# LOAD ENVIRONMENT
# ========================
BUILD_NUMBER = os.environ.get("BUILD_NUMBER", "0")
GIT_BRANCH   = os.environ.get("GIT_BRANCH", "unknown")
WORKSPACE    = r"D:\jenkins-agent\workspace\Team 11\Team Obsession\Gam300"
ENV_FILE     = os.path.join(WORKSPACE, "installer", "last_build.env")

# Read zip path/name written by build_installer.bat
zip_path = None
zip_name = None
if os.path.exists(ENV_FILE):
    with open(ENV_FILE, "r") as f:
        for line in f:
            if line.startswith("ZIP_PATH="):
                zip_path = line.strip().split("=", 1)[1]
            elif line.startswith("ZIP_NAME="):
                zip_name = line.strip().split("=", 1)[1]

if not zip_path or not os.path.exists(zip_path):
    print(f"ERROR: Installer zip not found. Expected path from last_build.env: {zip_path}")
    sys.exit(1)

print(f"=== Installer zip located: {zip_name} ({os.path.getsize(zip_path) // 1024} KB) ===")

mode = sys.argv[1] if len(sys.argv) > 1 else None

# ========================
# GITHUB RELEASE UPLOAD
# ========================
if mode == "github":
    tag = sys.argv[2] if len(sys.argv) > 2 else f"build-{BUILD_NUMBER}"

    GITHUB_TOKEN = os.environ.get("GITHUB_TOKEN")
    if not GITHUB_TOKEN:
        print("ERROR: GITHUB_TOKEN environment variable not set!")
        sys.exit(1)

    # Update this to your actual GitHub repo
    REPO_NAME = "InfamousJokim/BoomEngine"

    try:
        from github import Github, GithubException

        g = Github(GITHUB_TOKEN)
        repo = g.get_repo(REPO_NAME)

        release_name  = f"Build #{BUILD_NUMBER}"
        release_notes = (
            f"**Automated CI Build**\n\n"
            f"- **Build:** #{BUILD_NUMBER}\n"
            f"- **Branch:** {GIT_BRANCH}\n"
            f"- **Configurations:** Release + Debug\n"
            f"- **Projects:** BoomEngine, Editor, Runtime, GameScripts\n"
        )

        # Delete existing release/tag with same name if it exists
        try:
            existing = repo.get_release(tag)
            existing.delete_release()
            print(f"=== Deleted existing release: {tag} ===")
        except GithubException:
            pass  # No existing release, that's fine

        release = repo.create_git_release(
            tag=tag,
            name=release_name,
            message=release_notes,
            draft=False,
            prerelease=True
        )

        print(f"=== Uploading {zip_name} to GitHub release {tag} ===")
        release.upload_asset(zip_path, label=zip_name)
        print(f"=== GitHub upload complete: {release.html_url} ===")

    except Exception as e:
        print(f"ERROR: GitHub upload failed: {e}")
        sys.exit(1)

# ========================
# DROPBOX UPLOAD
# ========================
elif mode == "dropbox":
    DROPBOX_TOKEN = os.environ.get("DROPBOX_TOKEN")
    if not DROPBOX_TOKEN:
        print("ERROR: DROPBOX_TOKEN environment variable not set!")
        sys.exit(1)

    DROPBOX_DEST = f"/BoomEngine/GAM300/builds/{zip_name}"

    try:
        import dropbox
        from dropbox.files import WriteMode
        from dropbox.exceptions import ApiError

        dbx = dropbox.Dropbox(DROPBOX_TOKEN)

        file_size = os.path.getsize(zip_path)
        CHUNK_SIZE = 150 * 1024 * 1024  # 150 MB chunks

        print(f"=== Uploading {zip_name} to Dropbox at {DROPBOX_DEST} ===")

        with open(zip_path, "rb") as f:
            if file_size <= CHUNK_SIZE:
                # Small file — single upload
                dbx.files_upload(f.read(), DROPBOX_DEST, mode=WriteMode.overwrite)
            else:
                # Large file — chunked upload session
                upload_session = dbx.files_upload_session_start(f.read(CHUNK_SIZE))
                cursor = dropbox.files.UploadSessionCursor(
                    session_id=upload_session.session_id,
                    offset=f.tell()
                )
                commit = dropbox.files.CommitInfo(path=DROPBOX_DEST, mode=WriteMode.overwrite)

                while f.tell() < file_size:
                    remaining = file_size - f.tell()
                    if remaining <= CHUNK_SIZE:
                        dbx.files_upload_session_finish(f.read(remaining), cursor, commit)
                    else:
                        dbx.files_upload_session_append_v2(f.read(CHUNK_SIZE), cursor)
                        cursor.offset = f.tell()

        print(f"=== Dropbox upload complete: {DROPBOX_DEST} ===")

    except Exception as e:
        print(f"ERROR: Dropbox upload failed: {e}")
        sys.exit(1)

else:
    print(f"ERROR: Unknown mode '{mode}'. Use 'github <tag>' or 'dropbox'.")
    sys.exit(1)
