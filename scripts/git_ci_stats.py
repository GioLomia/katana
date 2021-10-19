#!/usr/bin/env python3
import argparse
import json
import os.path
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
import github

import requests
from requests.auth import HTTPBasicAuth
import time


def get_master(repo):
    return repo.get_branch(branch="master")


def get_branch(repo, branch_name):
    branch = repo.get_branch(branch=branch_name)
    return branch


def get_file_data(file_ls):
    full_file_data = {}
    for file in file_ls:
        sha = file.sha
        full_file_data[sha] = {}
        full_file_data[sha]["filename"] = file.filename
        full_file_data[sha]["raw_url"] = file.raw_url


def get_commit_data(commit):
    print(commit)
    commit_data = {}
    commit_data["sha"] = commit.sha
    commit_data["files"] = get_file_data(commit.files)
    commit_data["author"] = commit.author

    return commit_data


def unpack_commits(commits):
    commits_ls = []
    for i in range(100):
        commits_ls.append(commits[i])
    return commits_ls

def unpack_wf_runs(wf_runs):
    all_runs = {"MostRecentWorkflow" : wf_runs[0].id}
    i = 0

    for run in wf_runs:
        run_json = {}
        run_json["event"] = run.event
        run_json["head_branch"] = run.head_branch
        run_json["head_sha"] = run.head_sha
        response = requests.get(run.jobs_url).json()
        run_json["jobs"] = response
        print(run.jobs_url)
        all_runs[run.id] = run_json
        i+=1
        if i == 10:
            break

    return all_runs

def unpack_workflows(workflows):
    all_wf = {}
    for wf in workflows:
        print(wf.url)
        wf_runs = wf.get_runs()
        all_wf[wf.id] = unpack_wf_runs(wf_runs)
    return all_wf

def save_data(path, output_json):
    with open(f"{path}/workflows.json", "w") as output_file:
        json.dump(output_json, output_file, indent=4)

def load_data(path):
    return json.load(open(f"{path}/workflows.json"))

def main(args):
    g = github.Github(os.environ["GITHUB_TOKEN"])
    repo = g.get_repo(args.url)
    print(repo)

    data_path = f"{args.result_dir}/"
    if not args.load:
        print("Getting Data from GitHub...")
        workflows = repo.get_workflows()
        output_json = unpack_workflows(workflows)
        save_data(data_path,output_json)
        data = load_data(data_path)
    else:
        print(f"Loading Data from saved json at {data_path}")
        data = load_data(data_path)

    # print(data)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--url", help="Repo to query file changes.",
    )
    parser.add_argument(
        "--result-dir", default=None, help = "Directory to output file change information.",
    )
    parser.add_argument(
        "--load", dest="load", action="store_true", help = "Determines if we should load the data from disc or collect it through the GitHub API"
    )
    parser.add_argument(
        "--no-load", default="load", action="store_true", help = "Determines if we should load the data from disc or collect it through the GitHub API"
    )
    parser.set_defaults(load = False)
    args = parser.parse_args()
    main(args)
