#!/usr/bin/env python3
"""Check the files Git will publish, local Markdown links and Python syntax."""
import ast
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote

root = Path(__file__).resolve().parents[1]
result = subprocess.run(
    ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
    cwd=root, check=True, stdout=subprocess.PIPE,
)
paths = sorted(set(result.stdout.decode().split("\0")) - {""})
errors = []
required = ("README.md", "LICENSE", "CONTRIBUTING.md", "THIRD_PARTY_NOTICES.md",
            ".gitignore", ".github/workflows/ci.yml", "docs/building.md",
            "docs/testing.md", "docs/integration.md", "docs/component-development.md")
for name in required:
    if name not in paths:
        errors.append(f"Missing publication file: {name}")
blocked_suffixes = {".o", ".obj", ".a", ".so", ".exe", ".apk", ".dex", ".class",
                    ".p12", ".pfx", ".jks", ".keystore", ".key", ".pem", ".log", ".pid", ".pyc",
                    ".png", ".jpg", ".jpeg", ".gif", ".webp", ".avif", ".svg", ".bmp",
                    ".ico", ".tif", ".tiff", ".heic", ".mp4", ".mov", ".webm"}
blocked_parts = {"build", "cmakefiles", "__pycache__", ".code-review-graph", ".codex",
                 ".claude", ".agents", ".cursor", ".windsurf", ".clinerules", "screenshots"}
blocked_names = {"agents.md", "claude.md", "gemini.md", "codex.md", ".cursorrules",
                 ".windsurfrules", ".mcp.json", "copilot-instructions.md"}
for name in paths:
    path = root / name
    if (any(part.lower() in blocked_parts or part.lower().startswith(".aider") for part in Path(name).parts)
            or path.name.lower() in blocked_names
            or name in {"docs/verification.md", "docs/motion-audit.md"}
            or path.suffix.lower() in blocked_suffixes):
        errors.append(f"Excluded artifact selected for publication: {name}")
        continue
    if path.is_symlink():
        if not path.resolve().is_relative_to(root):
            errors.append(f"Symlink escapes repository: {name}")
        continue
    if not path.is_file():
        errors.append(f"Selected file is missing: {name}")
        continue
    content = path.read_text(encoding="utf-8")
    # Concatenate the token so this check does not match its own source.
    if re.search("-----BEGIN " + r"(?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----", content):
        errors.append(f"Private key found: {name}")
    if path.suffix == ".py":
        try:
            ast.parse(content, filename=name)
        except SyntaxError as error:
            errors.append(str(error))
    if path.suffix == ".md":
        # Code fences can contain C++ lambdas such as [](const Route &),
        # which are not Markdown links.
        prose = re.sub(r"(?ms)^[ \t]*(`{3,}|~{3,})[^\n]*\n.*?^[ \t]*\1[ \t]*(?:\n|$)", "", content)
        for target in re.findall(r"!?\[[^\]]+\]\(([^\s)]+)(?:\s+[^)]*)?\)", prose):
            if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("#"):
                continue
            target = unquote(target.split("#", 1)[0].split("?", 1)[0])
            linked = (path.parent / target).resolve()
            if not linked.is_relative_to(root) or not linked.exists():
                errors.append(f"Broken/outside local link in {name}: {target}")
            elif linked.is_file() and linked.relative_to(root).as_posix() not in paths:
                errors.append(f"Link points to an ignored/unpublished file in {name}: {target}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"PASS repository: {len(paths)} publishable files, local links and Python syntax")
