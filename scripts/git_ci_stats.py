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


def get_repo(args, g):
    g.get_repo(args.url)


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


def get_auth():
    try:
        password = os.environ.get(
            "GITHUB_PASSWORD", os.environ.get("GITHUB_TOKEN", None))
        if not password:
            raise KeyError()
        return HTTPBasicAuth(os.environ["GITHUB_USERNAME"], password)
    except KeyError:
        return None

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
    auth = get_auth()
    g = github.Github()
    repo = get_repo(args, g)
    commits = repo.get_commits()
    commits_ls = unpack_commits(commits)
    my_commit = commits_ls[5]
    commit_data = get_commit_data(my_commit)
    commit_json = json.dumps(commit_data, indent=4)