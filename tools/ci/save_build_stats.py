#!/usr/bin/env python3

"""
CREATE TABLE public.builds (
    id serial NOT NULL,
    pipeline_timestamp timestamp(0) NOT NULL,
    pipeline_url varchar NOT NULL,
    branch varchar NOT NULL,
    commit_sha1 varchar(40) NOT NULL,
    commit_timestamp timestamp(0) NOT NULL,
    "attributes" json NULL,
    CONSTRAINT builds_pk PRIMARY KEY (id)
);
"""

import json
import logging
import os
import re

import psycopg


logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


results = {}
results["commit_title"] = os.environ["CI_COMMIT_TITLE"]


# Benchmark

try:
    with open("benchmark-stdout.txt", "rt") as f:
        body = f.read()
        results["benchmarks"] = []

        for scenario, cycles_str in re.findall(r"\(benchmark ([^\s]+) (\d+)\)", body):
            d = dict(scenario=scenario, cycles=int(cycles_str))
            results["benchmarks"].append(d)
except FileNotFoundError:
    logger.exception("benchmark-stdout.txt not found")
except Exception:
    logger.exception("benchmark-stdout.txt malformed or test failed")


# ROM size

try:
    with open("rom-size.txt", "rt") as f:
        results["rom_size"] = int(f.read())
except FileNotFoundError:
    logger.exception("rom-size.txt not found")
except Exception:
    logger.exception("rom-size.txt malformed or test failed")


# ELF sections size

try:
    with open("elf-size.txt", "rt") as f:
        # skip headers
        next(f)
        next(f)

        results["elf_size"] = {}
        for row in f:
            tokens = row.split()
            if len(tokens) >= 2:
                results["elf_size"][tokens[0]] = int(tokens[1])
except Exception:
    logger.exception("elf-size.txt malformed")


# Connect to DB

with psycopg.connect(os.environ["POSTGRES_CONN_STRING"]) as conn:
    cursor = conn.cursor()
    cursor.execute('INSERT INTO public.builds(pipeline_timestamp, pipeline_url, branch, commit_sha1, commit_timestamp, "attributes") VALUES (%s, %s, %s, %s, %s, %s)', (
        os.environ["CI_PIPELINE_CREATED_AT"],
        os.environ["CI_PIPELINE_URL"],
        os.environ["CI_COMMIT_BRANCH"],
        os.environ["CI_COMMIT_SHA"],
        os.environ["CI_COMMIT_TIMESTAMP"],
        json.dumps(results))
    )
