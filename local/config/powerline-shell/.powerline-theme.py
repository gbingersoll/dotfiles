from powerline_shell.themes.default import DefaultColor

class Color(DefaultColor):
    """Theme that only uses colors in 0-15 range"""
    HOME_SPECIAL_DISPLAY = 3
    HOME_BG = 4
    HOME_FG = 15
    HOME_FG = 3
    PATH_BG = 8
    PATH_FG = 7
    CWD_FG = 15
    SEPARATOR_FG = 7

    READONLY_BG = 1
    READONLY_FG = 15

    REPO_CLEAN_BG = 2   # green
    REPO_CLEAN_FG = 0   # black
    REPO_DIRTY_BG = 1   # red
    REPO_DIRTY_FG = 15  # white

    GIT_AHEAD_FG = 15
    GIT_AHEAD_BG = 8
    GIT_BEHIND_FG = 15
    GIT_BEHIND_BG = 8
    GIT_STAGED_FG = 15
    GIT_STAGED_BG = 2
    GIT_NOTSTAGED_FG = 8
    GIT_NOTSTAGED_BG = 3
    GIT_UNTRACKED_FG = 15
    GIT_UNTRACKED_BG = 6
    GIT_CONFLICTED_FG = 8
    GIT_CONFLICTED_BG = 13

    JOBS_FG = 14
    JOBS_BG = 8

    CMD_PASSED_BG = 8
    CMD_PASSED_FG = 15
    CMD_FAILED_BG = 13
    CMD_FAILED_FG = 0

    SVN_CHANGES_BG = REPO_DIRTY_BG
    SVN_CHANGES_FG = REPO_DIRTY_FG

    VIRTUAL_ENV_BG = 2
    VIRTUAL_ENV_FG = 0

    AWS_PROFILE_FG = 14
    AWS_PROFILE_BG = 8

    TIME_FG = 8
    TIME_BG = 7
