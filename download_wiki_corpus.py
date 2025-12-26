#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import time
import csv
import re
import random
import requests
from collections import deque
from typing import Dict, List, Tuple, Set

API_URL = "https://en.wikipedia.org/w/api.php"

# -----------------------------
# Настройки
# -----------------------------
ROOT_CATEGORY = "Category:Video games"  # корневая категория на enwiki
TARGET_DOCS = 30000                    # для "на 3": 30000..50000
MAX_DEPTH = 6
MIN_CHARS = 200
OUT_DIR = "corpus"
META_PATH = "meta.tsv"

BATCH_SIZE = 50                        # можно 30..50; 100 рискованнее
REQUEST_PAUSE = 0.08

MAX_RETRIES = 6
BACKOFF_BASE = 0.6
BACKOFF_JITTER = 0.25


# -----------------------------
# Helpers
# -----------------------------
def safe_filename_id(i: int) -> str:
    return f"{i:06d}"


def clean_text(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def is_retryable_status(code: int) -> bool:
    return code in (429, 500, 502, 503, 504)


def api_get(params: dict, session: requests.Session) -> dict:
    last_err = None
    for attempt in range(MAX_RETRIES):
        try:
            r = session.get(API_URL, params=params, timeout=40)
            if r.status_code != 200 and is_retryable_status(r.status_code):
                raise requests.HTTPError(f"HTTP {r.status_code}", response=r)
            r.raise_for_status()
            return r.json()
        except (requests.Timeout, requests.ConnectionError, requests.HTTPError) as e:
            last_err = e
            sleep_s = (BACKOFF_BASE * (2 ** attempt)) + random.uniform(0, BACKOFF_JITTER)
            time.sleep(sleep_s)
    raise RuntimeError(f"API request failed after {MAX_RETRIES} retries: {last_err}")


def get_category_members(category_title: str, session: requests.Session) -> Tuple[List[Tuple[int, str]], List[str]]:
    pages: List[Tuple[int, str]] = []
    subcats: List[str] = []

    params = {
        "action": "query",
        "list": "categorymembers",
        "cmtitle": category_title,
        "cmlimit": "500",
        "cmtype": "page|subcat",
        "format": "json",
    }

    cont = {}
    while True:
        data = api_get({**params, **cont}, session)
        members = data.get("query", {}).get("categorymembers", [])
        for m in members:
            title = (m.get("title", "") or "")
            if title.startswith("Category:"):
                subcats.append(title)
            else:
                pid = m.get("pageid")
                if isinstance(pid, int):
                    pages.append((pid, title))

        if "continue" in data:
            cont = data["continue"]
        else:
            break

        time.sleep(REQUEST_PAUSE)

    return pages, subcats


def get_plaintext_extracts_batch(pageids: List[int], session: requests.Session) -> Dict[int, str]:
    if not pageids:
        return {}

    params = {
        "action": "query",
        "prop": "extracts",
        "explaintext": 1,
        "exsectionformat": "plain",
        "pageids": "|".join(str(pid) for pid in pageids),
        "format": "json",
    }
    data = api_get(params, session)
    pages = data.get("query", {}).get("pages", {})

    out: Dict[int, str] = {}
    for pid_str, page in pages.items():
        try:
            pid = int(pid_str)
        except ValueError:
            continue
        out[pid] = page.get("extract", "") or ""
    return out


def load_resume_state(meta_path: str, out_dir: str) -> Tuple[Set[str], int]:
    already_titles: Set[str] = set()
    doc_id = 0

    if os.path.exists(meta_path):
        with open(meta_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line or line.startswith("id\t"):
                    continue
                parts = line.split("\t")
                if len(parts) >= 2:
                    already_titles.add(parts[1])

    if os.path.isdir(out_dir):
        existing = [name for name in os.listdir(out_dir) if name.endswith(".txt")]
        if existing:
            try:
                doc_id = max(int(x.split(".")[0]) for x in existing)
            except Exception:
                doc_id = 0

    return already_titles, doc_id


# -----------------------------
# Main
# -----------------------------
def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    already_titles, doc_id = load_resume_state(META_PATH, OUT_DIR)

    visited_cats: Set[str] = set()
    q = deque([(ROOT_CATEGORY, 0)])

    session = requests.Session()
    session.headers.update({
        "User-Agent": "MAI-InfoSearch-Lab/1.0 (educational; contact: student)"
    })

    file_exists = os.path.exists(META_PATH)
    meta_f = open(META_PATH, "a", encoding="utf-8", newline="")
    writer = csv.writer(meta_f, delimiter="\t")
    if not file_exists:
        writer.writerow(["id", "title", "url", "bytes"])

    started = time.time()
    last_report_time = started
    last_report_docs = doc_id

    try:
        while q and doc_id < TARGET_DOCS:
            cat, depth = q.popleft()
            if cat in visited_cats:
                continue
            visited_cats.add(cat)

            pages, subcats = get_category_members(cat, session)
            print(f"Category: {cat} | pages: {len(pages)} | subcats: {len(subcats)} | depth: {depth}")

            if depth < MAX_DEPTH:
                for sc in subcats:
                    if sc not in visited_cats:
                        q.append((sc, depth + 1))

            batch_ids: List[int] = []
            batch_titles: List[Tuple[int, str]] = []

            def flush_batch():
                nonlocal doc_id, batch_ids, batch_titles
                if not batch_ids:
                    return

                extracts = get_plaintext_extracts_batch(batch_ids, session)

                for pid, title in batch_titles:
                    if doc_id >= TARGET_DOCS:
                        break

                    text = clean_text(extracts.get(pid, ""))
                    if len(text) < MIN_CHARS:
                        continue

                    doc_id += 1
                    fid = safe_filename_id(doc_id)
                    path = os.path.join(OUT_DIR, f"{fid}.txt")

                    with open(path, "w", encoding="utf-8") as out:
                        out.write(text + "\n")

                    url = "https://en.wikipedia.org/wiki/" + title.replace(" ", "_")
                    writer.writerow([fid, title, url, os.path.getsize(path)])
                    meta_f.flush()
                    already_titles.add(title)

                batch_ids = []
                batch_titles = []
                time.sleep(REQUEST_PAUSE)

            for pageid, title in pages:
                if doc_id >= TARGET_DOCS:
                    break
                if not pageid or not title:
                    continue
                if title in already_titles:
                    continue

                batch_ids.append(pageid)
                batch_titles.append((pageid, title))

                if len(batch_ids) >= BATCH_SIZE:
                    flush_batch()

                now = time.time()
                if now - last_report_time >= 10:
                    ddocs = doc_id - last_report_docs
                    dt = now - last_report_time
                    speed = (ddocs / dt) if dt > 0 else 0.0
                    print(f"Progress: {doc_id}/{TARGET_DOCS} | ~{speed:.2f} docs/s")
                    last_report_time = now
                    last_report_docs = doc_id

            flush_batch()

        total_time = time.time() - started
        print("\nDone.")
        print(f"Total documents: {doc_id}")
        print(f"Time: {total_time/60:.2f} min")
        print(f"Corpus folder: {OUT_DIR}/")
        print(f"Metadata: {META_PATH}")

    finally:
        meta_f.close()


if __name__ == "__main__":
    main()
