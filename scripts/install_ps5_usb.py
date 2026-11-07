#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Private fresh USB delivery. Default is read-only; never launches a game.

WFM endpoint comes only from --endpoint or PE3_WFM_ENDPOINT. No retries, removal,
overwrite, cross-directory move, internal-storage access or automatic cleanup.
"""
import argparse
import hashlib
import http.client
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from pathlib import Path, PurePosixPath

USB = "/mnt/usb0"
HOME = USB + "/homebrew"
TITLE = "PPSA77025"
APP = HOME + "/" + TITLE
DATA = USB + "/third-birthday-data"
ISO = DATA + "/ULUS10567.iso"
ISO_BYTES = 1467547648
BLOCK = 1024 * 1024
SCAN_MAX_DEPTH = 6
SCAN_MAX_DIRECTORIES = 256
SCAN_MAX_ENTRIES = 25000
SCAN_MAX_METADATA_BYTES = 1024 * 1024
KNOWN_CONTAINERS = {"PPSA35169-Compressed", "PS5VR-1.0.0-PPSA99177", "PS4",
                    "PS5SX2-vk-285-118", "PPSA31371-Compressed", "PPSA28997-Compressed"}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(BLOCK):
            value.update(block)
    return value.hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def leaf(name):
    require(isinstance(name, str) and name and name not in (".", "..") and
            not any(ord(c) < 32 or c in '/\\:*?"<>|' for c in name) and
            name[-1] not in ". " and len(name.encode("utf-8")) < 256,
            "Invalid filesystem component")
    return name


def local_candidate(manifest_path):
    manifest = read_json(manifest_path)
    state = read_json(manifest_path.parent / "STAGING-RESULT.json")
    require(state.get("state") == "READY_FOR_MANAGER_REVIEW" and state.get("private") is True,
            "Local finalized staging gate is not ready")
    require(manifest.get("applicationId") == TITLE and manifest.get("private") is True and
            manifest.get("nativeWritesEnabled") is False and state.get("nativeWritesEnabled") is False and
            manifest.get("iconIncluded") is True and state.get("iconIncluded") is True and
            manifest.get("isoIncluded") is False and manifest.get("saveTemplatesIncluded") is False,
            "Candidate identity/write/artwork/data policy mismatch")
    root = (manifest_path.parent / TITLE).resolve()
    require(Path(manifest["appRoot"]).resolve() == root, "Manifest app root mismatch")
    rows, folded = {}, set()
    for row in manifest["files"]:
        relative = row["path"]
        require(isinstance(relative, str) and not relative.startswith("/") and "\\" not in relative,
                "Invalid manifest relative path")
        parts = relative.split("/")
        for part in parts:
            leaf(part)
        require(parts[0] not in ("user", "memstick", "fs", "screenshots") and
                relative.casefold() not in folded, "User template or ambiguous manifest path")
        folded.add(relative.casefold())
        file = root.joinpath(*parts)
        require(not any(root.joinpath(*parts[:i]).is_symlink() for i in range(1, len(parts) + 1)),
                "Local symlink/reparse input is unsupported")
        require(file.is_file() and type(row["bytes"]) is int and row["bytes"] >= 0 and
                re.fullmatch(r"[0-9a-fA-F]{64}", row["sha256"]), "Malformed staged file record")
        require(file.stat().st_size == row["bytes"] and digest(file) == row["sha256"].lower(),
                "Local staged file differs from reviewed manifest: " + relative)
        rows[relative] = dict(source=file, size=row["bytes"], sha256=row["sha256"].lower())
    actual = {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()}
    require(set(rows) == actual and len(rows) == state.get("fileCount"), "Staged file inventory mismatch")
    require({"eboot.bin", "sce_sys/param.json", "sce_sys/icon0.png"} <= set(rows), "Required app files missing")
    require(rows["eboot.bin"]["sha256"] == manifest["reviewedEbootSHA256"] == state["ebootSHA256"] and
            rows["sce_sys/param.json"]["sha256"] == state["paramSHA256"], "Finalized hash gate mismatch")
    metadata = read_json(rows["sce_sys/param.json"]["source"])
    require(metadata.get("titleId") == TITLE and isinstance(metadata.get("contentId"), str), "Metadata identity mismatch")
    return rows, metadata


class Wfm:
    def __init__(self, endpoint, execute):
        self.base = endpoint.rstrip("/")
        self.url = urllib.parse.urlsplit(self.base)
        require(self.url.scheme in ("http", "https") and self.url.hostname and
                not self.url.username and not self.url.password and self.url.path in ("", "/") and
                not self.url.query and not self.url.fragment, "Endpoint must be a plain HTTP(S) origin")
        self.execute = execute

    def request(self, route, params=None, post=False, mutate=False):
        require(not mutate or self.execute, "Read-only mode refuses mutation")
        query = urllib.parse.urlencode(params or {})
        request = urllib.request.Request(self.base + route + ("" if post else "?" + query),
                                         data=query.encode() if post else None)
        try:
            response = urllib.request.urlopen(request, timeout=60)
            require(response.status == 200, "Unexpected WFM HTTP status")
            return response
        except (urllib.error.URLError, OSError) as error:
            raise RuntimeError("WFM request unavailable; remote state is unknown") from None

    def api(self, route, params=None, post=False, mutate=False):
        with self.request(route, params, post, mutate) as response:
            body = response.read(4 * BLOCK + 1)
        require(len(body) <= 4 * BLOCK, "WFM response exceeds supported schema bounds")
        data = json.loads(body)
        require(isinstance(data, dict) and data.get("ok") is True, "WFM API reported failure or unknown envelope")
        return data

    def listing(self, directory):
        data = self.api("/api/list", {"path": directory})
        require(data.get("path") == directory and isinstance(data.get("entries"), list), "Unconfirmed listing path/schema")
        result = {}
        for row in data["entries"]:
            require(isinstance(row, dict), "Invalid listing entry")
            name = leaf(row.get("name"))
            require(name not in result and row.get("type") in ("-", "d", "l"), "Duplicate/unknown directory entry")
            if "path" in row:
                require(row["path"] == directory + "/" + name, "Listing entry escaped requested directory")
            if row["type"] == "-":
                require(type(row.get("size")) is int and row["size"] >= 0, "Unknown remote file size")
            result[name] = row
        return result

    def metadata(self, path, expected_size):
        require(0 < expected_size <= 65536, "Unsupported metadata size")
        with self.request("/api/text", {"path": path}) as response:
            body = response.read(65537)
        require(len(body) == expected_size, "Metadata length changed or read schema unsupported")
        value = json.loads(body.decode("utf-8-sig"))
        require(isinstance(value, dict) and isinstance(value.get("titleId"), str) and
                value["titleId"].strip() and isinstance(value.get("contentId"), str) and
                value["contentId"].strip(), "Unrecognized app metadata; collision state unknown")
        return value

    def absent(self, parent, name):
        require(name not in self.listing(parent), "Destination already exists; preserve it and request backup/update review")

    def mkdir(self, parent, name):
        self.absent(parent, name)
        self.api("/api/mkdir", {"path": parent, "name": leaf(name)}, mutate=True)
        require(self.listing(parent).get(name, {}).get("type") == "d", "Created directory not positively confirmed")

    def rename_sibling(self, source, name):
        parent, old = source.rsplit("/", 1)
        self.absent(parent, name)
        self.api("/api/rename", {"path": source, "name": leaf(name)}, mutate=True)
        entries = self.listing(parent)
        require(old not in entries and entries.get(name, {}).get("type") == "-", "Sibling file rename result unknown")

    def task_id(self, data):
        value = data.get("task_id")
        require(type(value) is int and value > 0, "Unknown WFM task identifier")
        return value

    def wait(self, task_id):
        deadline = time.monotonic() + 300
        while time.monotonic() < deadline:
            rows = self.api("/api/tasks").get("tasks")
            require(isinstance(rows, list), "Unknown task-list envelope")
            found = [r for r in rows if isinstance(r, dict) and r.get("id") == task_id]
            require(len(found) == 1, "Upload task disappeared/ambiguous; completion unknown")
            status = found[0].get("state")
            if status == "done":
                return
            require(status in ("queued", "running"), "Upload task failed/canceled or unknown state")
            time.sleep(0.5)  # Task observation only; no transfer-operation retries.
        raise RuntimeError("Upload completion timed out; preserve unfinished staging")

    def verify_download(self, remote, expected):
        task = self.task_id(self.api("/api/download/prepare", {"paths": remote}, post=True, mutate=True))
        value, total = hashlib.sha256(), 0
        with self.request("/api/download", {"id": task}) as response:
            while block := response.read(BLOCK):
                total += len(block)
                require(total <= expected["size"], "Readback exceeds expected file size")
                value.update(block)
        require(total == expected["size"] and value.hexdigest() == expected["sha256"], "Remote readback hash/length mismatch")
        self.api("/api/tasks", {"download_ack": task}, mutate=True)

    def upload_verified(self, directory, final_name, expected, report, save):
        require(self.execute, "Read-only mode refuses upload")
        self.absent(directory, final_name)
        temporary = ".pe3-upload-" + uuid.uuid4().hex + ".part"
        self.absent(directory, temporary)
        task = self.task_id(self.api("/api/upload/prepare", dict(path=directory, src=temporary,
                            total=expected["size"], count=1, rels=temporary,
                            sizes=str(expected["size"]), overwrite="0"), post=True, mutate=True))
        report["unfinished"] = dict(path=directory + "/" + temporary, task=task)
        save()
        connection_type = http.client.HTTPSConnection if self.url.scheme == "https" else http.client.HTTPConnection
        connection = connection_type(self.url.hostname, self.url.port, timeout=60)
        try:
            connection.putrequest("POST", "/api/upload-file")
            headers = {"Content-Type": "application/octet-stream", "Content-Length": str(expected["size"]),
                       "X-WFM-Task-ID": str(task), "X-WFM-Path": urllib.parse.quote(directory, safe=""),
                       "X-WFM-Rel": urllib.parse.quote(temporary, safe=""), "X-WFM-Size": str(expected["size"]),
                       "X-WFM-Overwrite": "0"}
            for key, value in headers.items():
                connection.putheader(key, value)
            connection.endheaders()
            value, total = hashlib.sha256(), 0
            with expected["source"].open("rb") as source:
                while block := source.read(BLOCK):
                    total += len(block)
                    require(total <= expected["size"], "Local upload source grew after review")
                    value.update(block)
                    connection.send(block)
            require(total == expected["size"] and value.hexdigest() == expected["sha256"], "Local upload source changed")
            response = connection.getresponse()
            body = response.read(BLOCK + 1)
            require(response.status == 200 and len(body) <= BLOCK and json.loads(body).get("ok") is True,
                    "Upload response failed/unknown")
        except (OSError, http.client.HTTPException):
            raise RuntimeError("Upload transport failed; preserve unfinished staging") from None
        finally:
            connection.close()
        self.api("/api/upload/finish", {"task_id": task}, mutate=True)
        self.wait(task)
        entry = self.listing(directory).get(temporary, {})
        require(entry.get("type") == "-" and entry.get("size") == expected["size"], "Uploaded file metadata mismatch")
        self.verify_download(directory + "/" + temporary, expected)
        self.rename_sibling(directory + "/" + temporary, final_name)
        entry = self.listing(directory).get(final_name, {})
        require(entry.get("type") == "-" and entry.get("size") == expected["size"], "Renamed file metadata mismatch")
        report["files"].append(dict(path=directory + "/" + final_name, bytes=expected["size"], sha256=expected["sha256"]))
        report.pop("unfinished", None)
        save()


def collision_scan(client, root_entries, metadata):
    """Directory discovery only; metadata is the only file content inspected.

    Exact destination absence is a separate gate. Unrelated incomplete packaging
    is an explicit coverage gap, not a conflict or proof of global ID absence.
    Stop at app indicators; no archive unpacking or link traversal is permitted.
    """
    cache = {HOME: root_entries}
    stats = dict(scope="selected-usb0-homebrew-live-directory-apps", directories=1,
                 entries=len(root_entries), metadata_bytes=0, identified_apps=0,
                 known_containers=0, ordinary_containers=0, excluded=[],
                 global_reservation=False)

    def exclude(path, reason, identity_known=False):
        selected_name = path.rsplit("/", 1)[-1].casefold() == TITLE.casefold()
        require(not selected_name or identity_known, "Selected app identity is unknown: " + path)
        packaging = any(part in KNOWN_CONTAINERS or part.startswith(".orbit-bundle-")
                        for part in path[len(HOME) + 1:].split("/"))
        prefix = "Incomplete/excluded packaging coverage: " if packaging else "Excluded unrelated coverage: "
        stats["excluded"].append(dict(path=path, reason=prefix + reason))

    def names(entries):
        folded = {name.casefold(): name for name in entries}
        require(len(folded) == len(entries), "Ambiguous case-folded directory entries")
        return folded

    def listing(path):
        require(path == HOME or path.startswith(HOME + "/"), "Collision scan escaped selected homebrew root")
        require(path == HOME or len(path[len(HOME) + 1:].split("/")) <= SCAN_MAX_DEPTH,
                "Collision scan depth exhausted; scope unknown")
        if path not in cache:
            require(stats["directories"] < SCAN_MAX_DIRECTORIES, "Collision directory budget exhausted; scope unknown")
            try:
                entries = client.listing(path)
            except RuntimeError:
                raise RuntimeError("Collision discovery listing unavailable; scope unknown: " + path) from None
            stats["directories"] += 1
            stats["entries"] += len(entries)
            require(stats["entries"] <= SCAN_MAX_ENTRIES, "Collision entry budget exhausted; scope unknown")
            cache[path] = entries
        return cache[path]

    def visit(path, depth):
        require(depth <= SCAN_MAX_DEPTH, "Collision scan depth exhausted; scope unknown")
        entries = listing(path)
        folded = names(entries)
        binary_name, system_name = folded.get("eboot.bin"), folded.get("sce_sys")
        parameter_name, system = None, None
        if system_name:
            if entries[system_name]["type"] != "d":
                exclude(path, "system-directory indicator is not a regular directory; no traversal")
                return
            system = listing(path + "/" + system_name)
            parameter_name = names(system).get("param.json")
        if binary_name or parameter_name:
            if parameter_name is None or system[parameter_name]["type"] != "-":
                exclude(path, "executable/app indicator has no readable regular metadata; not inspected or unpacked")
                return
            size = system[parameter_name].get("size")
            require(type(size) is int and 0 < size <= 65536, "App metadata size outside supported bound")
            stats["metadata_bytes"] += size
            require(stats["metadata_bytes"] <= SCAN_MAX_METADATA_BYTES, "Collision metadata-byte budget exhausted; scope unknown")
            try:
                other = client.metadata(path + "/" + system_name + "/" + parameter_name, size)
            except (RuntimeError, ValueError, KeyError, TypeError):
                exclude(path, "metadata could not be read/qualified; identity excluded from successful-read scope")
                return
            require(other["titleId"].casefold() != TITLE.casefold() and
                    other["contentId"].casefold() != metadata["contentId"].casefold(), "Title/content ID collision: " + path)
            stats["identified_apps"] += 1
            if binary_name is None or entries[binary_name]["type"] != "-":
                exclude(path, "metadata identity read, but regular executable is absent; no payload inspection", identity_known=True)
            return  # Do not inspect this app's assets, saves, archives or payloads.
        basename = path.rsplit("/", 1)[-1]
        require(basename.casefold() != TITLE.casefold(), "Selected nested app directory has unknown identity: " + path)
        known = basename in KNOWN_CONTAINERS or basename.startswith(".orbit-bundle-")
        stats["known_containers" if known else "ordinary_containers"] += 1
        for name, row in entries.items():
            if row["type"] == "l":
                exclude(path + "/" + name, "link not traversed")
            elif row["type"] == "d":
                visit(path + "/" + name, depth + 1)

    require(stats["entries"] <= SCAN_MAX_ENTRIES, "Collision root entry budget exhausted")
    names(root_entries)
    for name, row in root_entries.items():
        if row["type"] == "l":
            exclude(HOME + "/" + name, "homebrew link not traversed")
        elif row["type"] == "d":
            visit(HOME + "/" + name, 1)
    return stats


def preflight(client, metadata):
    usb = client.listing(USB)
    require(usb.get("homebrew", {}).get("type") == "d", "Existing USB homebrew anchor unavailable")
    apps = client.listing(HOME)
    require(not any(name.casefold() == TITLE.casefold() for name in apps),
            "App destination exists; backup/update review required; no overwrite")
    result = collision_scan(client, apps, metadata)
    if "third-birthday-data" in usb:
        require(usb["third-birthday-data"]["type"] == "d", "Owned ISO directory collides with a non-directory")
        require(not any(name.casefold() == "ULUS10567.iso".casefold() for name in client.listing(DATA)),
                "ISO destination exists; preserve it and review separately")
    return result


def verify_app_tree(client, rows, include_metadata):
    expected = {}
    selected = set(rows) if include_metadata else set(rows) - {"sce_sys/param.json"}
    for relative in selected:
        parts = relative.split("/")
        parent = ""
        for part in parts[:-1]:
            expected.setdefault(parent, {})[part] = ("d", None)
            parent = parent + "/" + part if parent else part
        expected.setdefault(parent, {})[parts[-1]] = ("-", rows[relative]["size"])
    for relative, wanted in expected.items():
        actual = client.listing(APP + ("/" + relative if relative else ""))
        require(set(actual) == set(wanted), "Unexpected/missing files in fresh app tree; preserve incomplete delivery")
        for name, (kind, size) in wanted.items():
            require(actual[name]["type"] == kind and (size is None or actual[name].get("size") == size),
                    "Final app inventory type/size mismatch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--endpoint", default=os.environ.get("PE3_WFM_ENDPOINT"))
    parser.add_argument("--iso", type=Path)
    parser.add_argument("--iso-sha256")
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--project-install-confirmed", action="store_true")
    # Legacy invocation compatibility only; current human instruction does not
    # require unrelated games to be closed. Never inspect/close/reboot them.
    parser.add_argument("--all-games-closed-confirmed", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    report = dict(state="local-preflight", execute=args.execute, files=[], app=APP, iso=ISO,
                  native_writes_enabled=False, launched=False, registered=False, installed_verified=False)
    require(not args.receipt.exists(), "Fresh private receipt path required")
    def save():
        args.receipt.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    try:
        if args.execute:
            require(args.project_install_confirmed, "Execute requires fresh explicit project-install confirmation")
            require(args.endpoint and args.iso and args.iso_sha256, "Execute requires endpoint and reviewed owned ISO/hash")
        rows, metadata = local_candidate(args.manifest.resolve())
        iso = None
        if args.iso:
            require(args.iso.is_file() and not args.iso.is_symlink() and args.iso.stat().st_size == ISO_BYTES and
                    args.iso_sha256 and re.fullmatch(r"[0-9a-fA-F]{64}", args.iso_sha256), "Owned ISO identity/size receipt incomplete")
            require(digest(args.iso) == args.iso_sha256.lower(), "Owned ISO differs from reviewed hash")
            iso = dict(source=args.iso, size=ISO_BYTES, sha256=args.iso_sha256.lower())
        report["planned_files"] = list(sorted(rows))
        report["manifest_sha256"] = digest(args.manifest)
        client = Wfm(args.endpoint, args.execute) if args.endpoint else None
        if client:
            report["collision_scan"] = preflight(client, metadata)
        report["state"] = "read-only-plan-remote-checked" if client else "read-only-plan-remote-unknown"
        save()
        if not args.execute:
            print("Read-only plan recorded; no file/directory mutation or launch.")
            return 0
        require(iso is not None, "Reviewed ISO source required")
        client.mkdir(HOME, TITLE)
        report["state"] = "fresh-app-created-metadata-absent"
        save()
        # Cinder-reviewed recoverable order: companions first, EBOOT next, metadata LAST.
        directories = {APP}
        def parent_for(relative):
            parent = APP
            for part in PurePosixPath(relative).parts[:-1]:
                destination = parent + "/" + part
                if destination not in directories:
                    client.mkdir(parent, part)
                    directories.add(destination)
                parent = destination
            return parent
        for relative in sorted(set(rows) - {"eboot.bin", "sce_sys/param.json"}):
            client.upload_verified(parent_for(relative), relative.rsplit("/", 1)[-1], rows[relative], report, save)
        client.upload_verified(APP, "eboot.bin", rows["eboot.bin"], report, save)
        usb = client.listing(USB)
        if "third-birthday-data" not in usb:
            client.mkdir(USB, "third-birthday-data")
        else:
            require(usb["third-birthday-data"]["type"] == "d", "ISO directory changed")
        client.upload_verified(DATA, "ULUS10567.iso", iso, report, save)
        verify_app_tree(client, rows, include_metadata=False)
        client.absent(APP + "/sce_sys", "param.json")
        client.upload_verified(APP + "/sce_sys", "param.json", rows["sce_sys/param.json"], report, save)
        verify_app_tree(client, rows, include_metadata=True)
        report["state"] = "files-delivered-and-readback-verified-not-launched-or-registration-proven"
        save()
        print("Files delivered and readback verified; no launch or registration/playability claim.")
        return 0
    except Exception as error:
        # Do not print endpoint-bearing exception text or perform automatic cleanup.
        report["state"] = "stopped-preserve-incomplete-files"
        report["failure_type"] = type(error).__name__
        report["reason"] = str(error) if isinstance(error, RuntimeError) else "Schema/I/O failure; remote state remains unknown"
        save()
        print("Transfer stopped; review private receipt and preserve incomplete files.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
