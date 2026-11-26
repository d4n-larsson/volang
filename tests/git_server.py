#!/usr/bin/env python3
"""The git host of spec 061 (research R8): repositories served over HTTPS for the package step cases.

  git_server.py --cert NAME --repos DIR [OPTION...]
  git_server.py --self-test

DIR holds one folder per repository and, in it, one folder per version: DIR/money/1.2.0/... is the
content of `money` at the tag v1.2.0 (annotated when the patch number is even, lightweight when odd; a
folder named 1.2.0-plain gives the tag 1.2.0 without the v). A file DIR/NAME/tags lists further tag
names, one per line, put on the last commit (tags that are not versions). `{port}` in a file named
vo.packages becomes the server's port and `{hash:NAME@VERSION}` the content hash of that folder; a file named links holds `NAME TARGET` lines, made symbolic
links in its folder. Commits have a fixed author and date.

Serves, for each NAME (also as NAME.git), the two requests of git's smart HTTP protocol by running
`git upload-pack --stateless-rpc`; everything else goes to tests/http_server.py's routes. Prints
"port N" when listening. OPTIONs make a repository hostile:
  truncate=NAME   the pack answer cut in half
  badsum=NAME     the pack's last byte flipped
  huge=NAME       a 17 MiB ref answer
  redirect=NAME   /NAME/... answers 301 to /NAME.git/...
  status=NAME:N   every request for NAME answers status N
"""
import hashlib
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import http_server  # noqa: E402
import https_server  # noqa: E402

ENV = dict(os.environ, GIT_AUTHOR_NAME="vo", GIT_AUTHOR_EMAIL="vo@example.com", GIT_COMMITTER_NAME="vo",
           GIT_COMMITTER_EMAIL="vo@example.com", GIT_AUTHOR_DATE="2026-01-01T00:00:00Z",
           GIT_COMMITTER_DATE="2026-01-01T00:00:00Z", GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null")
REPOS = {}  # name -> directory of its git repository
OPTS = {}   # option -> set of names (status: name -> code)


def git(cwd, *args, data=None):
    return subprocess.run(["git", *args], cwd=cwd, env=ENV, input=data, capture_output=True, check=True).stdout


def version_key(folder):
    return [int(x) for x in folder.split("-")[0].split(".")]


def content_hash(folder):
    """The package content hash of the .vo files under folder, as `vo package` computes it."""
    files = []
    for d, _, names in os.walk(folder):
        files += [os.path.relpath(os.path.join(d, n), folder) for n in names if n.endswith(".vo") and len(n) > 3]
    h = hashlib.sha256()
    for rel in sorted(files, key=lambda r: r.encode()):
        data = open(os.path.join(folder, rel), "rb").read()
        h.update(rel.encode() + b"\0" + str(len(data)).encode() + b"\0" + data)
    return "sha256:" + h.hexdigest()


def manifest_text(text, repos, port):
    """`{port}`, and `{hash:NAME@VERSION}`: the content hash of that version's folder."""
    text = text.replace("{port}", str(port))
    return re.sub(r"\{hash:(\w+)@([\w.-]+)\}", lambda m: content_hash(os.path.join(repos, m.group(1), m.group(2))), text)


def build(repos, port, into):
    for name in sorted(os.listdir(repos)):
        src = os.path.join(repos, name)
        if not os.path.isdir(src):
            continue
        work = os.path.join(into, name)
        os.makedirs(work)
        git(work, "init", "-q", "-b", "main")
        versions = sorted((v for v in os.listdir(src) if re.fullmatch(r"\d+\.\d+\.\d+(-plain)?", v)), key=version_key)
        for v in versions:
            for old in os.listdir(work):
                if old != ".git":
                    p = os.path.join(work, old)
                    shutil.rmtree(p) if os.path.isdir(p) and not os.path.islink(p) else os.remove(p)
            shutil.copytree(os.path.join(src, v), work, dirs_exist_ok=True, symlinks=True)
            for d, _, files in os.walk(work):
                if "links" in files and ".git" not in d:  # `NAME TARGET` lines: symbolic links to make
                    for line in open(os.path.join(d, "links")).read().splitlines():
                        os.symlink(line.split()[1], os.path.join(d, line.split()[0]))
                    os.remove(os.path.join(d, "links"))
                if "vo.packages" in files and ".git" not in d:
                    p = os.path.join(d, "vo.packages")
                    text = manifest_text(open(p).read(), repos, port)
                    open(p, "w").write(text)
            index = os.path.join(work, ".git", "index")
            if os.path.exists(index):  # no trust in file times: versions are written in the same instant
                os.remove(index)
            git(work, "add", "-A")
            git(work, "commit", "-q", "--allow-empty", "-m", v)
            num = v.split("-")[0]
            tag = num if v.endswith("-plain") else "v" + num
            if int(num.split(".")[2]) % 2 == 0:
                git(work, "tag", "-a", "-m", tag, tag)
            else:
                git(work, "tag", tag)
        if not versions:  # a repository without versions still has a commit
            git(work, "commit", "-q", "--allow-empty", "-m", "start")
        extra = os.path.join(src, "tags")
        if os.path.isfile(extra):
            for tag in open(extra).read().split():
                git(work, "tag", tag)
        REPOS[name] = work


def route(method, target, headers, body):
    path, _, query = target.partition("?")
    m = re.fullmatch(r"/([A-Za-z0-9_-]+?)(\.git)?/(info/refs|git-upload-pack)", path)
    if not m or m.group(1) not in REPOS:
        return None
    name, dotgit, what = m.groups()
    if name in OPTS.get("status", {}):
        return http_server.reply(f"{OPTS['status'][name]} Status", [("Content-Length", 0)], b"")
    if name in OPTS.get("redirect", ()) and not dotgit:
        to = f"/{name}.git/{what}" + ("?" + query if query else "")
        return http_server.reply("301 Moved Permanently", [("Location", to), ("Content-Length", 0)], b"")
    repo = REPOS[name]
    if what == "info/refs" and method == "GET" and query == "service=git-upload-pack":
        out = b"001e# service=git-upload-pack\n0000" + git(repo, "upload-pack", "--stateless-rpc", "--advertise-refs", ".")
        if name in OPTS.get("huge", ()):
            out += b"0" * (17 << 20)
        kind = "application/x-git-upload-pack-advertisement"
    elif what == "git-upload-pack" and method == "POST":
        out = git(repo, "upload-pack", "--stateless-rpc", ".", data=body)
        if name in OPTS.get("truncate", ()):
            out = out[:len(out) // 2]
        if name in OPTS.get("badsum", ()):
            out = out[:-1] + bytes([out[-1] ^ 1])
        kind = "application/x-git-upload-pack-result"
    else:
        return http_server.reply("400 Bad Request", [("Content-Length", 0)], b"")
    return http_server.reply("200 OK", [("Content-Type", kind), ("Content-Length", len(out)), ("Connection", "close")], out)


def start(cert, repos, options):
    """Builds the repositories and serves them; returns (port, temporary directory)."""
    for o in options:
        k, _, v = o.partition("=")
        if k == "status":
            n, _, code = v.partition(":")
            OPTS.setdefault("status", {})[n] = int(code)
        else:
            OPTS.setdefault(k, set()).add(v)
    a = type("A", (), dict(cert=cert, tls10_only=False, tls12_only=False, tls13_only=False, tamper=False, cipher=None))
    ctx = https_server.openssl_context(a)
    s = https_server.listen(0)
    port = s.getsockname()[1]
    tmp = tempfile.mkdtemp(prefix="vo-git-")
    build(repos, port, tmp)
    plain = http_server.route
    http_server.route = lambda m, t, h, b: route(m, t, h, b) or plain(m, t, h, b)
    wrap = lambda c: ctx.wrap_socket(c, server_side=True)  # noqa: E731
    threading.Thread(target=https_server.accept_loop, args=(s, wrap, https_server.logger(None)), daemon=True).start()
    return port, tmp


def self_test():
    src = tempfile.mkdtemp(prefix="vo-git-src-")
    for v, text in (("1.0.0", "a"), ("1.0.1", "b"), ("1.1.0-plain", "c")):
        os.makedirs(os.path.join(src, "money", v, "sub"))
        open(os.path.join(src, "money", v, "sub", "x.vo"), "w").write(text)
    port, tmp = start("a.test", src, [])
    env = dict(ENV, GIT_SSL_NO_VERIFY="1")
    out = tempfile.mkdtemp(prefix="vo-git-out-")
    bad = 0
    for tag, text in (("v1.0.0", "a"), ("v1.0.1", "b"), ("1.1.0", "c")):
        to = os.path.join(out, tag)
        subprocess.run(["git", "clone", "-q", "--depth", "1", "--branch", tag, f"https://127.0.0.1:{port}/money", to],
                       env=env, capture_output=True, check=True)
        if open(os.path.join(to, "sub", "x.vo")).read() != text:
            print(f"FAIL {tag}")
            bad = 1
    for d in (src, tmp, out):
        shutil.rmtree(d)
    print("git_server self-test:", "FAIL" if bad else "ok")
    return bad


def main():
    if sys.argv[1:] == ["--self-test"]:
        sys.exit(self_test())
    args = sys.argv[1:]
    cert, repos = args[args.index("--cert") + 1], args[args.index("--repos") + 1]
    options = [a for a in args if "=" in a]
    port, tmp = start(cert, repos, options)
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))  # steps.py terminates it: remove the repositories
    print(f"port {port}", flush=True)
    try:
        threading.Event().wait()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
