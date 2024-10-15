#!/usr/bin/env python3

import json
import os
from pathlib import Path
import sys

import jinja2
import psycopg


with psycopg.connect(os.environ["POSTGRES_CONN_STRING"]) as conn:
    cursor = conn.cursor()
    cursor.execute('SELECT id, pipeline_timestamp, pipeline_url, branch, commit_sha1, commit_timestamp, "attributes" '
                   'FROM public.builds ORDER BY pipeline_timestamp DESC, id DESC')
    # TODO: can fetch directly as dict?
    builds = [dict(id=row[0],
                   pipeline_timestamp=row[1],
                   pipeline_url=row[2],
                   branch=row[3],
                   commit_sha1=row[4],
                   commit_timestamp=row[5],
                   **row[6]) for row in cursor.fetchall()]

env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(os.path.dirname(__file__)),
    trim_blocks=True,
    lstrip_blocks=True,
)

template = env.get_template("build_stats.html")

Path("builds.html").write_text(template.render(
    builds=builds,
    project_url=os.environ["CI_PROJECT_URL"]
    ))

# Generate badges
# TODO: these should take information from "last successful build on master" or something like that, rather than just last build overall
# reference_build = builds[0]
# reference_clk = "$glbnet$clk_sys"
#
# try:
#     fmax_str = "%.1f MHz" % reference_build["build"]["fmax"][reference_clk]["achieved"]
# except:
#     fmax_str = "unknown"
#
# try:
#     dmips_str = "%.1f DMIPS" % reference_build["benchmark"]["dmips"]
# except:
#     dmips_str = "unknown"
#
# Path("fmax.json").write_text(json.dumps(dict(schemaVersion=1, label="Fmax", message=fmax_str, color="orange")))
# Path("dmips.json").write_text(json.dumps(dict(schemaVersion=1, label="Benchmark", message=dmips_str, color="blueviolet")))
