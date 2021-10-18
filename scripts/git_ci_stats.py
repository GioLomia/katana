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

def unpack_check_suite(suite):
    for run in suite:
        print("         ", run.name)


def main(args):
    g = github.Github(os.environ["GITHUB_TOKEN"])
    repo = g.get_repo(args.url)
    print(repo)
    commits = repo.get_commits()
    commits_ls = unpack_commits(commits)
    my_commit = commits_ls[1]
    commit_check_suite = my_commit.get_check_suites()
    commit_run_check = my_commit.get_check_runs()
    print(my_commit, commit_run_check)
    
    for suite in commit_check_suite:
        unpack_check_suite(suite.get_check_runs())

    # for check in commit_run_check:
    #     print(check.name)
    #     annotations = check.get_annotations()
    #     # unpack_check_run(annotations)
    #     print()

        # unpack_check_suite(check)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--url", help="Repo to query file changes.",
    )
    parser.add_argument(
        "--result-dir", help="Directory to output file change information.",
    )
    args = parser.parse_args()
    main(args)
