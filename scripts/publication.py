"""Shared source-publication exclusions; no filesystem or Git side effects."""
from pathlib import PurePosixPath
import re

blocked_suffixes = {".o", ".obj", ".a", ".so", ".exe", ".apk", ".dex", ".class",
                    ".p12", ".pfx", ".jks", ".keystore", ".key", ".pem", ".log", ".pid", ".pyc",
                    ".png", ".jpg", ".jpeg", ".gif", ".webp", ".avif", ".svg", ".bmp",
                    ".ico", ".tif", ".tiff", ".heic", ".mp4", ".mov", ".webm"}
blocked_parts = {"build", "cmakefiles", "__pycache__", ".code-review-graph", ".codex",
                 ".claude", ".agents", ".cursor", ".windsurf", ".clinerules", "screenshots"}
blocked_names = {"agents.md", "claude.md", "gemini.md", "codex.md", ".cursorrules",
                 ".windsurfrules", ".mcp.json", "copilot-instructions.md"}
def excluded(name):
    path = PurePosixPath(name)
    return (any(part.lower() in blocked_parts or part.lower().startswith(".aider") for part in path.parts)
            or path.name.lower() in blocked_names
            or name in {"docs/verification.md", "docs/motion-audit.md"}
            or path.suffix.lower() in blocked_suffixes)


def has_private_key(content):
    # Keep the marker split so checks do not flag their own implementation.
    return re.search("-----BEGIN " + r"(?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----", content) is not None
