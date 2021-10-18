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

def unpack_check_suite(check_suite):
    # for app in check_suite.
    print(check_suite.app)

def get_auth():
    try:
        password = os.environ.get(
            "GITHUB_PASSWORD", os.environ.get("GITHUB_TOKEN", None))
        if not password:
            raise KeyError()
        return HTTPBasicAuth(os.environ["GITHUB_USERNAME"], password)
    except KeyError:
        return None



def main(args):
    # auth = get_auth()
    # if not auth:
    #     print(
    #         "This script requires GITHUB_USERNAME and either GITHUB_PASSWORD "
    #         "or GITHUB_TOKEN to be set to valid Github credentials."
    #     )
    #     return 2
    # page = 0
    # repo_prefix = f"https://api.github.com/repos/{args.url}"
    # response = requests.get(
    #     f"{repo_prefix}/actions/runs",
    #     params={"branch": "master", "status": "success"},
    #     headers={"Accept": "application/vnd.github.v3+json"},
    #     auth=auth,
    # )
    # print(response.json())


    g = github.Github(os.environ["GITHUB_TOKEN"])
    repo = g.get_repo(args.url)
    print(repo)
    commits = repo.get_commits()
    commits_ls = unpack_commits(commits)
    my_commit = commits_ls[0]
    commit_run_check = my_commit.get_check_runs()
    print(my_commit, commit_run_check)

    for check in commit_run_check:
        print(check.name)
        print(check)
        unpack_check_suite(check)


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
